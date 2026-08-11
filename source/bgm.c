#include"bgm.h"
#include<string.h>
#include"common.h"

// This is to differentiate all the places where we use "6" to meant number
// of strings on a guitar versus "6" being used for some other purpose.
#define STRING_COUNT          6

// Number of PDSynth and channels to allocate for each string.
//
// If we play two notes quickly in succession using the same synth, the old
// note should have ended before the new note started due to synth envelope,
// but what we have observed is that the old note sometimes get cutoff
// abruptly, which leads to a clipping sound.  I tried working around this
// by inserting a short delay between notes to guarantee that synth will cut
// off each note before the new notes cut them off, but this cause some
// rapid notes to become too short, and that would result in some clipping
// sounds as well.
//
// Rather than timing the notes to guarantee release between successive
// notes, we just play each note on new synth, and old note is allowed to
// overlap a bit with the new note.  This appears to remove all blips.
//
// Earlier project also had separate channels for each string, but turns
// out the separation of synth objects is all we needed, and we can just
// play everything on the default channel.
#define SYNTH_PER_STRING      2

// Note velocities.
#define FULL_VELOCITY         1.0f
#define REDUCED_VELOCITY      0.7f

// Collection of handles associated with a single instrument.
typedef struct
{
   AudioSample *sample;
   PDSynth *synth[SYNTH_PER_STRING];
   SoundChannel *channel[SYNTH_PER_STRING];
} Instrument;

// A single musical note.
typedef struct
{
   // Start timestamp of this note in milliseconds.  Negative timestamp
   // marks the end of the song.
   int32_t time_ms;

   // Duration of this note in milliseconds.
   //
   // Absolute value of this field encodes the duration.  The sign encodes
   // whether the note is to be played a full velocity (negative) or
   // reduced velocity (positive).
   //
   // If zero, note is silent.  This is used to encode the final duration
   // of silence at the end of the song, before the negative timestamp.
   int16_t duration_ms;

   // Number of semitones away from open string.
   //
   // This assumes standard EADGBE tuning.  In alternative DADGBE where 6th
   // string is dropped to D, this value may be negative.
   int16_t note;
} SongNote;

typedef struct
{
   const SongNote *track[STRING_COUNT];
} Song;

#include"gnossienne.c"

// Collection of all songs.
static const Song *g_song[] =
{
   &g_gnossienne,
};

// All loaded instruments.
static Instrument g_instrument[STRING_COUNT];

// Currently selected sample set.
static int g_sample_index = -1;

// Song position data.
static uint32_t g_track_position[STRING_COUNT];

// Load or change instruments.
void InitBgm(PlaydateAPI *pd, int sample_index)
{
   assert(sample_index >= 0);
   if( g_sample_index == sample_index )
      return;
   g_sample_index = sample_index;
   #ifndef NDEBUG
      pd->system->logToConsole("InitBgm(sample_index=%d)", sample_index);
   #endif

   // Initialize guitar strings.
   for(int i = 0; i < STRING_COUNT; i++)
   {
      const char sample_name[4] =
      {
         'm',
         '1' + sample_index,
         '1' + i,
         0
      };
      if( g_instrument[i].sample != NULL )
      {
         // Replace currently loaded sample.
         for(int j = 0; j < SYNTH_PER_STRING; j++)
            pd->sound->synth->noteOff(g_instrument[i].synth[j], 0);
         pd->sound->sample->loadIntoSample(g_instrument[i].sample, sample_name);
         continue;
      }
      else
      {
         // Load new sample.
         g_instrument[i].sample = pd->sound->sample->load(sample_name);
         assert(g_instrument[i].sample != NULL);
      }

      for(int j = 0; j < SYNTH_PER_STRING; j++)
      {
         g_instrument[i].synth[j] = pd->sound->synth->newSynth();
         assert(g_instrument[i].synth[j] != NULL);
         pd->sound->synth->setSample(
            g_instrument[i].synth[j], g_instrument[i].sample, 0, 0);
         pd->sound->synth->setVolume(g_instrument[i].synth[j], 1, 1);
      }
   }
}

// Rewind song state to the beginning.
void RewindBgm(void)
{
   memset(g_track_position, 0, sizeof(g_track_position));
}

// Play all notes up to song_time_ms, inclusive.
void PlayBgm(PlaydateAPI *pd, int song_index, int32_t song_time_ms)
{
   for(int i = 0; i < STRING_COUNT; i++)
   {
      const SongNote *track = g_song[song_index]->track[i];
      uint32_t j = g_track_position[i];
      for(; track[j].time_ms >= 0; j++)
      {
         if( track[j].time_ms > song_time_ms )
            break;

         // Play note using PDSynth.
         //
         // "60" here is interpreted as "play at the original sample
         // rate".  Each guitar string is tuned to a different note
         // and none of them are C4, we are just using the delta from
         // C4 to adjust the relative note pitch.
         if( track[j].duration_ms > 0 )
         {
            pd->sound->synth->playMIDINote(
               g_instrument[i].synth[j % SYNTH_PER_STRING],
               track[j].note + 60,
               REDUCED_VELOCITY,
               track[j].duration_ms / 1000.0f,
               0);
         }
         else if( track[j].duration_ms < 0 )
         {
            pd->sound->synth->playMIDINote(
               g_instrument[i].synth[j % SYNTH_PER_STRING],
               track[j].note + 60,
               FULL_VELOCITY,
               track[j].duration_ms / -1000.0f,
               0);
         }
      }
      g_track_position[i] = j;
   }
}

// Check if current song has completed, returns 1 if so.
int BgmCompleted(int song_index)
{
   for(int i = 0; i < STRING_COUNT; i++)
   {
      const SongNote *track = g_song[song_index]->track[i];
      int j = g_track_position[i];
      if( track[j].time_ms >= 0 )
         return 0;
   }
   return 1;
}
