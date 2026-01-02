#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    struct NoteEvent
    {
        int noteNumber = 0;
        int channel = 1;
        double onTime = 0.0;
        double offTime = 0.0;
    };

    struct Stats
    {
        double min = std::numeric_limits<double>::infinity();
        double max = -std::numeric_limits<double>::infinity();
        double sum = 0.0;
        double sumAbs = 0.0;
        int count = 0;

        void add (double value)
        {
            min = std::min (min, value);
            max = std::max (max, value);
            sum += value;
            sumAbs += std::abs (value);
            ++count;
        }

        double mean() const
        {
            return count > 0 ? sum / static_cast<double> (count) : 0.0;
        }

        double meanAbs() const
        {
            return count > 0 ? sumAbs / static_cast<double> (count) : 0.0;
        }
    };

    struct MatchStats
    {
        int totalUserNotes = 0;
        int totalReferenceNotes = 0;
        int matchedInWindow = 0;
        int missedMatches = 0;
        int referenceExhausted = 0;
        Stats deltaMatched;
    };

    double convertTicksToSeconds (double time,
                                  const juce::MidiMessageSequence& tempoEvents,
                                  int timeFormat)
    {
        if (timeFormat < 0)
            return time / (-(timeFormat >> 8) * (timeFormat & 0xff));

        double lastTime = 0.0;
        double correctedTime = 0.0;
        const auto tickLen = 1.0 / static_cast<double> (timeFormat & 0x7fff);
        double secsPerTick = 0.5 * tickLen;
        const auto numEvents = tempoEvents.getNumEvents();

        for (int i = 0; i < numEvents; ++i)
        {
            auto& message = tempoEvents.getEventPointer (i)->message;
            const auto eventTime = message.getTimeStamp();

            if (eventTime >= time)
                break;

            correctedTime += (eventTime - lastTime) * secsPerTick;
            lastTime = eventTime;

            if (message.isTempoMetaEvent())
                secsPerTick = tickLen * message.getTempoSecondsPerQuarterNote();

            while (i + 1 < numEvents)
            {
                auto& next = tempoEvents.getEventPointer (i + 1)->message;
                if (! juce::approximatelyEqual (next.getTimeStamp(), eventTime))
                    break;

                if (next.isTempoMetaEvent())
                    secsPerTick = tickLen * next.getTempoSecondsPerQuarterNote();

                ++i;
            }
        }

        return correctedTime + (time - lastTime) * secsPerTick;
    }

    bool loadMidiFile (const juce::File& file,
                       juce::MidiFile& midiFile,
                       juce::MidiMessageSequence& combined,
                       juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = "File not found: " + file.getFullPathName();
            return false;
        }

        juce::FileInputStream stream (file);
        if (! stream.openedOk())
        {
            error = "Unable to open file: " + file.getFullPathName();
            return false;
        }

        if (! midiFile.readFrom (stream))
        {
            error = "Invalid MIDI file: " + file.getFullPathName();
            return false;
        }

        combined.clear();

        for (int i = 0; i < midiFile.getNumTracks(); ++i)
            combined.addSequence (*midiFile.getTrack (i), 0.0);

        combined.sort();
        combined.updateMatchedPairs();
        return true;
    }

    void buildTempoEvents (const juce::MidiFile& midiFile,
                           juce::MidiMessageSequence& tempoEvents)
    {
        tempoEvents.clear();
        midiFile.findAllTempoEvents (tempoEvents);
        midiFile.findAllTimeSigEvents (tempoEvents);
        tempoEvents.sort();
    }

    void convertSequenceToSeconds (const juce::MidiMessageSequence& source,
                                   const juce::MidiMessageSequence& tempoEvents,
                                   int timeFormat,
                                   double tickScale,
                                   juce::MidiMessageSequence& dest)
    {
        dest.clear();

        const auto numEvents = source.getNumEvents();
        for (int i = 0; i < numEvents; ++i)
        {
            auto message = source.getEventPointer (i)->message;
            const double tickTime = message.getTimeStamp() * tickScale;
            message.setTimeStamp (convertTicksToSeconds (tickTime, tempoEvents, timeFormat));
            dest.addEvent (message);
        }

        dest.sort();
        dest.updateMatchedPairs();
    }

    std::vector<NoteEvent> extractNotes (const juce::MidiMessageSequence& sequence)
    {
        std::vector<NoteEvent> notes;
        notes.reserve (static_cast<size_t> (sequence.getNumEvents()));

        const auto numEvents = sequence.getNumEvents();
        for (int i = 0; i < numEvents; ++i)
        {
            const auto* event = sequence.getEventPointer (i);
            if (event == nullptr)
                continue;

            const auto& message = event->message;
            if (! message.isNoteOn())
                continue;

            const auto* noteOffEvent = event->noteOffObject;
            if (noteOffEvent == nullptr)
                continue;

            const auto& offMessage = noteOffEvent->message;
            NoteEvent note;
            note.noteNumber = message.getNoteNumber();
            note.channel = message.getChannel();
            note.onTime = message.getTimeStamp();
            note.offTime = offMessage.getTimeStamp();
            notes.push_back (note);
        }

        std::sort (notes.begin(), notes.end(),
                   [] (const NoteEvent& a, const NoteEvent& b)
                   {
                       return a.onTime < b.onTime;
                   });
        return notes;
    }

    double findFirstNoteTime (const std::vector<NoteEvent>& notes)
    {
        if (notes.empty())
            return 0.0;
        return notes.front().onTime;
    }

    MatchStats simulateMatching (const std::vector<NoteEvent>& referenceNotes,
                                 const std::vector<NoteEvent>& userNotes,
                                 double matchWindowSeconds,
                                 double correction,
                                 bool alignToFirstNote)
    {
        MatchStats stats;
        stats.totalReferenceNotes = static_cast<int> (referenceNotes.size());
        stats.totalUserNotes = static_cast<int> (userNotes.size());

        const double referenceStart = alignToFirstNote ? findFirstNoteTime (referenceNotes) : 0.0;
        const double userStart = alignToFirstNote ? findFirstNoteTime (userNotes) : 0.0;

        std::vector<uint8_t> matched (referenceNotes.size(), 0);
        int referenceCursor = 0;

        for (const auto& userNote : userNotes)
        {
            const double userTime = userNote.onTime - userStart;

            if (referenceCursor >= static_cast<int> (referenceNotes.size()))
            {
                ++stats.referenceExhausted;
                continue;
            }

            int selectedIndex = -1;
            bool matchedInWindow = false;

            if (matchWindowSeconds <= 0.0)
            {
                selectedIndex = referenceCursor;
                matchedInWindow = true;
            }
            else
            {
                const double windowStart = userTime - matchWindowSeconds;
                const double windowEnd = userTime + matchWindowSeconds;
                int bestIndex = -1;
                double bestDistance = 0.0;
                int firstCandidateIndex = -1;

                for (int i = referenceCursor; i < static_cast<int> (referenceNotes.size()); ++i)
                {
                    if (matched[static_cast<size_t> (i)] != 0)
                        continue;

                    const auto& refNote = referenceNotes[static_cast<size_t> (i)];
                    const double alignedRefTime = refNote.onTime - referenceStart;

                    if (alignedRefTime < windowStart)
                        continue;
                    if (firstCandidateIndex < 0)
                        firstCandidateIndex = i;
                    if (alignedRefTime > windowEnd)
                        break;
                    if (refNote.noteNumber != userNote.noteNumber || refNote.channel != userNote.channel)
                        continue;

                    const double distance = std::abs (alignedRefTime - userTime);
                    if (bestIndex < 0 || distance < bestDistance)
                    {
                        bestIndex = i;
                        bestDistance = distance;
                    }
                }

                if (bestIndex >= 0)
                {
                    selectedIndex = bestIndex;
                    matchedInWindow = true;
                }
                else
                {
                    if (firstCandidateIndex >= 0 && firstCandidateIndex > referenceCursor)
                        referenceCursor = firstCandidateIndex;
                    ++stats.missedMatches;
                    continue;
                }
            }

            if (selectedIndex < 0 || selectedIndex >= static_cast<int> (referenceNotes.size()))
                continue;

            matched[static_cast<size_t> (selectedIndex)] = 1;
            while (referenceCursor < static_cast<int> (referenceNotes.size())
                && matched[static_cast<size_t> (referenceCursor)] != 0)
            {
                ++referenceCursor;
            }

            const auto& refNote = referenceNotes[static_cast<size_t> (selectedIndex)];
            const double alignedRefTime = refNote.onTime - referenceStart;
            const double rawDeltaSeconds = alignedRefTime - userTime;
            const double deltaSeconds = rawDeltaSeconds * correction;
            const double deltaMs = deltaSeconds * 1000.0;

            if (matchedInWindow)
                ++stats.matchedInWindow;
            stats.deltaMatched.add (deltaMs);
        }

        return stats;
    }

    void printStats (const MatchStats& stats)
    {
        std::cout << "Reference notes: " << stats.totalReferenceNotes << "\n";
        std::cout << "User notes: " << stats.totalUserNotes << "\n";
        std::cout << "Matched in window: " << stats.matchedInWindow << "\n";
        std::cout << "Missed matches: " << stats.missedMatches << "\n";
        std::cout << "Reference exhausted: " << stats.referenceExhausted << "\n";
        if (stats.deltaMatched.count > 0)
        {
            std::cout << "Delta (ms) matched: min " << stats.deltaMatched.min
                      << ", max " << stats.deltaMatched.max
                      << ", mean " << stats.deltaMatched.mean()
                      << ", mean abs " << stats.deltaMatched.meanAbs() << "\n";
        }
    }

    void printUsage()
    {
        std::cout << "Usage: Personalities_OfflineMatchSim [options]\n"
                  << "  --reference <file> (default assets/reference_performance.mid)\n"
                  << "  --user <file>      (default assets/user_performance.mid)\n"
                  << "  --match-window-ms <value> (default 60)\n"
                  << "  --slack-ms <value>        (default 50)\n"
                  << "  --correction <value>      (default 1.0)\n"
                  << "  --user-tempo <reference|file>\n"
                  << "  --user-bpm <value> (implies fixed tempo)\n"
                  << "  --no-align-start (use absolute timestamps)\n";
    }
}

int main (int argc, char* argv[])
{
    juce::File referenceFile = juce::File::getCurrentWorkingDirectory()
        .getChildFile ("assets")
        .getChildFile ("reference_performance.mid");
    juce::File userFile = juce::File::getCurrentWorkingDirectory()
        .getChildFile ("assets")
        .getChildFile ("user_performance.mid");

    double matchWindowMs = 60.0;
    double slackMs = 50.0;
    double correction = 1.0;
    bool alignToFirstNote = true;
    enum class UserTempoMode { Reference, File, Fixed };
    UserTempoMode userTempoMode = UserTempoMode::Reference;
    double userFixedBpm = 120.0;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg = argv[i];
        if (arg == "--reference" && i + 1 < argc)
        {
            referenceFile = juce::File (argv[++i]);
        }
        else if (arg == "--user" && i + 1 < argc)
        {
            userFile = juce::File (argv[++i]);
        }
        else if (arg == "--match-window-ms" && i + 1 < argc)
        {
            matchWindowMs = juce::String (argv[++i]).getDoubleValue();
        }
        else if (arg == "--slack-ms" && i + 1 < argc)
        {
            slackMs = juce::String (argv[++i]).getDoubleValue();
        }
        else if (arg == "--correction" && i + 1 < argc)
        {
            correction = juce::String (argv[++i]).getDoubleValue();
        }
        else if (arg == "--user-tempo" && i + 1 < argc)
        {
            const juce::String mode = argv[++i];
            if (mode == "file")
                userTempoMode = UserTempoMode::File;
            else
                userTempoMode = UserTempoMode::Reference;
        }
        else if (arg == "--user-bpm" && i + 1 < argc)
        {
            userFixedBpm = juce::String (argv[++i]).getDoubleValue();
            userTempoMode = UserTempoMode::Fixed;
        }
        else if (arg == "--no-align-start")
        {
            alignToFirstNote = false;
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return 0;
        }
    }

    juce::MidiFile referenceMidi;
    juce::MidiFile userMidi;
    juce::MidiMessageSequence referenceCombined;
    juce::MidiMessageSequence userCombined;
    juce::String error;

    if (! loadMidiFile (referenceFile, referenceMidi, referenceCombined, error))
    {
        std::cerr << error << "\n";
        return 1;
    }

    if (! loadMidiFile (userFile, userMidi, userCombined, error))
    {
        std::cerr << error << "\n";
        return 1;
    }

    juce::MidiMessageSequence referenceTempoEvents;
    buildTempoEvents (referenceMidi, referenceTempoEvents);

    juce::MidiMessageSequence userTempoEvents;
    if (userTempoMode == UserTempoMode::File)
        buildTempoEvents (userMidi, userTempoEvents);

    juce::MidiMessageSequence fixedTempoEvents;
    if (userTempoMode == UserTempoMode::Fixed)
    {
        const auto microsecondsPerQuarter = static_cast<int> (std::lround (60000000.0 / userFixedBpm));
        fixedTempoEvents.addEvent (juce::MidiMessage::tempoMetaEvent (microsecondsPerQuarter));
    }

    const int referenceTimeFormat = referenceMidi.getTimeFormat();
    const int userTimeFormat = userMidi.getTimeFormat();
    double userTickScale = 1.0;

    if (userTempoMode == UserTempoMode::Reference
        && referenceTimeFormat > 0
        && userTimeFormat > 0)
    {
        const double refTpq = static_cast<double> (referenceTimeFormat & 0x7fff);
        const double userTpq = static_cast<double> (userTimeFormat & 0x7fff);
        if (userTpq > 0.0)
            userTickScale = refTpq / userTpq;
    }

    juce::MidiMessageSequence referenceSeconds;
    convertSequenceToSeconds (referenceCombined, referenceTempoEvents, referenceTimeFormat, 1.0, referenceSeconds);

    juce::MidiMessageSequence userSeconds;
    if (userTempoMode == UserTempoMode::Reference)
    {
        convertSequenceToSeconds (userCombined, referenceTempoEvents, referenceTimeFormat, userTickScale, userSeconds);
    }
    else if (userTempoMode == UserTempoMode::Fixed)
    {
        convertSequenceToSeconds (userCombined, fixedTempoEvents, userTimeFormat, 1.0, userSeconds);
    }
    else
    {
        convertSequenceToSeconds (userCombined, userTempoEvents, userTimeFormat, 1.0, userSeconds);
    }

    const auto referenceNotes = extractNotes (referenceSeconds);
    const auto userNotes = extractNotes (userSeconds);

    std::cout << "Reference file: " << referenceFile.getFullPathName() << "\n";
    std::cout << "User file: " << userFile.getFullPathName() << "\n";
    std::cout << "Match window: " << matchWindowMs << " ms\n";
    std::cout << "Slack: " << slackMs << " ms (does not affect matching)\n";
    std::cout << "Correction: " << correction << "\n";
    std::cout << "Align to first note: " << (alignToFirstNote ? "yes" : "no") << "\n";
    std::cout << "User tempo mode: "
              << (userTempoMode == UserTempoMode::Reference ? "reference"
                  : (userTempoMode == UserTempoMode::Fixed ? "fixed" : "file"))
              << "\n";
    if (userTempoMode == UserTempoMode::Fixed)
        std::cout << "User BPM: " << userFixedBpm << "\n";
    std::cout << "\n";

    const auto stats = simulateMatching (referenceNotes,
                                         userNotes,
                                         matchWindowMs / 1000.0,
                                         correction,
                                         alignToFirstNote);
    printStats (stats);

    return 0;
}
