/*
 *  DOSBox MP3 Seek Table Handler
 *  -----------------------------
 *
 * Problem:
 *          Seeking within an MP3 file to an exact time-offset, such as is expected
 *          within DOS games, is extremely difficult because the MP3 format doesn't
 *          provide a defined relationship between the compressed data stream positions
 *          versus decompressed PCM times.
 *
 * Solution:
 *          To solve this, we step through each compressed MP3 frames in
 *          the MP3 file (without decoding the actual audio) and keep a record of the
 *          decompressed "PCM" times for each frame.  We save this relationship to
 *          to a local fie, called a fast-seek look-up table, which we can quickly
 *          reuse every subsequent time we need to seek within the MP3 file.  This allows
 *          seeks to be performed extremely fast while being PCM-exact.
 *
 *          This "fast-seek" file can hold data for multiple MP3s to avoid
 *          creating an excessive number of files in the local working directory.
 *
 * Challenges:
 *       1. What happens if an MP3 file is changed but the MP3's filename remains the same?
 *
 *          The lookup table is indexed based on a checksum instead of filename.
 *          The checksum is calculated based on a subset of the MP3's content in
 *          addition to being seeded based on the MP3's size in bytes.
 *          This makes it very sensitive to changes in MP3 content; if a change
 *          is detected a new lookup table is generated.
 *
 *       2. Checksums can be weak, what if a collision happens?
 *
 *          To avoid the risk of collision, we use the current best-of-breed
 *          xxHash algorithm that has a quality-score of 10, the highest rating
 *          from the SMHasher test set.  See https://github.com/Cyan4973/xxHash
 *          for more details.
 *
 *       3. What happens if fast-seek file is brought from a little-endian
 *          machine to a big-endian machine (x86 or ARM to a PowerPC or Sun
 *          Sparc machine)?
 *
 *          The lookup table is serialized and multi-byte types are byte-swapped
 *          at runtime according to the architecture. This makes fast-seek files
 *          cross-compatible regardless of where they were written to or read from.
 *
 *       4. What happens if this code is updated to use a new fast-seek file
 *          format, but an old fast-seek file exists?
 *
 *          The seek-table file is versioned (see SEEK_TABLE_IDENTIFIER befow),
 *          therefore, if the format and version is updated, then the seek-table
 *          will be regenerated.

 * The seek table handler makes use of the following single-header public libraries:
 *   - dr_mp3:   http://mackron.github.io/dr_mp3.html, by David Reid
 *   - archive:  https://github.com/voidah/archive, by Arthur Ouellet
 *   - xxHash:   http://cyan4973.github.io/xxHash, by Yann Collet
 *
 *  Copyright (C) 2020       The DOSBox Team
 *  Copyright (C) 2018-2019  Kevin R. Croft <krcroft@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

// System headers
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <map>

// Local headers
#include "support.h"

#define XXH_INLINE_ALL
#include "xxhash.h"
// #include "../../../include/logging.h"
#include "mp3_seek_table.h"

// C++ scope modifiers
using std::map;
using std::vector;
using std::string;
using std::ios_base;
using std::ifstream;
using std::ofstream;

// Identifies a valid versioned seek-table
#define SEEK_TABLE_IDENTIFIER "st-v4"

// How many compressed MP3 frames should we skip between each recorded
// time point.  The trade-off is as follows:
//   - a large number means slower in-game seeking but a smaller fast-seek file.
//   - a smaller numbers (below 10) results in fast seeks on slow hardware.
constexpr uint32_t FRAMES_PER_SEEK_POINT = 7;

// Returns the size of a file in bytes (if valid), otherwise -1
off_t get_file_size(const char* filename) {
    struct stat stat_buf;
    int rc = stat(filename, &stat_buf);
    return (rc >= 0) ? stat_buf.st_size : -1;
}


// Calculates a unique 64-bit hash (integer) from the provided file.
// This function should not cause side-effects; ie, the current
// read-position within the file should not be altered.
//
// This function tries to files as-close to the middle of the MP3 file as possible,
// and use that feed the hash function in hopes of the most uniqueness.
// We're trying to avoid content that might be duplicated across MP3s, like:
// 1. ID3 tag filler content, which might be boiler plate or all empty
// 2. Trailing silence or similar zero-PCM content
//
Uint64 calculate_stream_hash(struct SDL_RWops* const context) {
    // Save the current stream position, so we can restore it at the end of the function.
    const Sint64 original_pos = SDL_RWtell(context);

    // Seek to the end of the file so we can calculate the stream size.
    SDL_RWseek(context, 0, RW_SEEK_END);

    const Sint64 stream_size = SDL_RWtell(context);
    if (stream_size <= 0) {
        // LOG_MSG("MP3: get_stream_size returned %d, but should be positive", stream_size);
        return 0;
    }

    // Seek to the middle of the file while taking into account version small files.
    const Sint64 tail_size = (stream_size > 32768) ? 32768 : stream_size;
    const Sint64 mid_pos = static_cast<Sint64>(stream_size/2.0) - tail_size;
    SDL_RWseek(context, mid_pos >= 0 ? (int)mid_pos : 0, RW_SEEK_SET);

    // Prepare our read buffer and counter:
    vector<char> buffer(1024, 0);
    size_t total_bytes_read = 0;

    // Initialize xxHash's state using the stream_size as our seed.
    // Seeding with the stream_size provide a second level of uniqueness
    // in the unlikely scenario that two files of different length happen to
    // have the same trailing 32KB of content.  The different seeds will produce
    // unique hashes.
    XXH64_state_t* const state = XXH64_createState();
    if(!state) {
        return 0;
    }

    const uint64_t seed = static_cast<uint64_t>(stream_size);
    XXH64_reset(state, seed);

    while (total_bytes_read < static_cast<size_t>(tail_size)) {
        // Read a chunk of data.
        const size_t bytes_read = SDL_RWread(context, buffer.data(), 1, (int)buffer.size());

        if (bytes_read != 0) {
            // Update our hash if we read data.
            XXH64_update(state, buffer.data(), bytes_read);
            total_bytes_read += bytes_read;
        } else {
            break;
        }
    }

    // restore the stream position
    SDL_RWseek(context, (int)original_pos, RW_SEEK_SET);

    const Uint64 hash = XXH64_digest(state);
    XXH64_freeState(state);
    return hash;
}

// This function generates a new seek-table for a given mp3 stream and writes
// the data to the fast-seek file.
//
Uint64 generate_new_seek_points(const char* filename,
                                const Uint64& stream_hash,
                                drmp3* const p_dr,
                                map<Uint64, vector<drmp3_seek_point_serial> >& seek_points_table,
                                map<Uint64, drmp3_uint64>& pcm_frame_count_table,
                                vector<drmp3_seek_point_serial>& seek_points_vector) {

    // Initialize our frame counters with zeros.
    drmp3_uint64 mp3_frame_count(0);
    drmp3_uint64 pcm_frame_count(0);

    // Get the number of compressed MP3 frames and the number of uncompressed PCM frames.
    drmp3_bool32 result = drmp3_get_mp3_and_pcm_frame_count(p_dr,
                                                            &mp3_frame_count,
                                                            &pcm_frame_count);

    if (   result != DRMP3_TRUE
        || mp3_frame_count < FRAMES_PER_SEEK_POINT
        || pcm_frame_count < FRAMES_PER_SEEK_POINT) {
        // LOG_MSG("MP3: failed to determine or find sufficient mp3 and pcm frames");
        return 0;
    }

    // Based on the number of frames found in the file, we size our seek-point
    // vector accordingly. We then pass our sized vector into dr_mp3 which populates
    // the decoded PCM times.
    // We also take into account the desired number of "FRAMES_PER_SEEK_POINT",
    // which is defined above.
    drmp3_uint32 num_seek_points = static_cast<drmp3_uint32>
        (ceil_udivide(mp3_frame_count, FRAMES_PER_SEEK_POINT));

    seek_points_vector.resize(num_seek_points);
    result = drmp3_calculate_seek_points(p_dr,
                                         &num_seek_points,
                                         reinterpret_cast<drmp3_seek_point*>(seek_points_vector.data()));

    if (result != DRMP3_TRUE || num_seek_points == 0) {
        // LOG_MSG("MP3: failed to calculate sufficient seek points for stream");
        return 0;
    }

    // The calculate function provides us with the actual number of generated seek
    // points in the num_seek_points variable; so if this differs from expected then we
    // need to resize (ie: shrink) our vector.
    if (num_seek_points != seek_points_vector.size()) {
        seek_points_vector.resize(num_seek_points);
    }

    // Update our lookup table file with the new seek points and pcm_frame_count.
    // Note: the serializer elegantly handles C++ STL objects and is endian-safe.
    seek_points_table[stream_hash] = seek_points_vector;
    pcm_frame_count_table[stream_hash] = pcm_frame_count;
    ofstream outfile(filename, ios_base::trunc | ios_base::binary);

    // Caching our seek table to file is optional.  If the user is blocked due to
    // security or write-access issues, then this write-phase is skipped. In this
    // scenario the seek table will be generated on-the-fly on every start of DOSBox.
    if (outfile.is_open()) {
        Archive<ofstream> serialize(outfile);
        serialize << SEEK_TABLE_IDENTIFIER << seek_points_table << pcm_frame_count_table;
        outfile.close();
    }

    // Finally, we return the number of decoded PCM frames for this given file, which
    // doubles as a success-code.
    return pcm_frame_count;
}

// This function attempts to fetch a seek-table for a given mp3 stream from the fast-seek file.
// If anything is amiss then this function fails.
//
Uint64 load_existing_seek_points(const char* filename,
                                 const Uint64& stream_hash,
                                 map<Uint64, vector<drmp3_seek_point_serial> >& seek_points_table,
                                 map<Uint64, drmp3_uint64>& pcm_frame_count_table,
                                 vector<drmp3_seek_point_serial>& seek_points) {

    // The below sentinals sanity check and read the incoming
    // file one-by-one until all the data can be trusted.

    // Sentinal 1: bail if we got a zero-byte file.
    struct stat buffer;
    if (stat(filename, &buffer) != 0) {
        return 0;
    }

    // Sentinal 2: Bail if the file isn't big enough to hold our identifier.
    if (get_file_size(filename) < static_cast<int64_t>(sizeof(SEEK_TABLE_IDENTIFIER))) {
        return 0;
    }

    // Sentinal 3: Bail if we don't get a matching identifier.
    string fetched_identifier;
    ifstream infile(filename, ios_base::binary);
    Archive<ifstream> deserialize(infile);
    deserialize >> fetched_identifier;
    if (fetched_identifier != SEEK_TABLE_IDENTIFIER) {
        infile.close();
        return 0;
    }

    // De-serialize the seek point and pcm_count tables.
    deserialize >> seek_points_table >> pcm_frame_count_table;
    infile.close();

    // Sentinal 4: does the seek_points table have our stream's hash?
    const auto p_seek_points = seek_points_table.find(stream_hash);
    if (p_seek_points == seek_points_table.end()) {
        return 0;
    }

    // Sentinal 5: does the pcm_frame_count table have our stream's hash?
    const auto p_pcm_frame_count = pcm_frame_count_table.find(stream_hash);
    if (p_pcm_frame_count == pcm_frame_count_table.end()) {
        return 0;
    }

    // If we made it here, the file was valid and has lookup-data for our
    // our desired stream
    seek_points = p_seek_points->second;
    return p_pcm_frame_count->second;
}

// This function attempts to populate our seek table for the given mp3 stream, first
// attempting to read it from the fast-seek file and (if it can't be read for any reason), it
// calculates new data.  It makes use of the above two functions.
//
uint64_t populate_seek_points(struct SDL_RWops* const context,
                              mp3_t* p_mp3,
                              const char* seektable_filename,
                              bool &result) {

    // assume failure until proven otherwise
    result = false;

    // Calculate the stream's xxHash value.
    Uint64 stream_hash = calculate_stream_hash(context);
    if (stream_hash == 0) {
        // LOG_MSG("MP3: could not compute the hash of the stream");
        return 0;
    }

    // Attempt to fetch the seek points and pcm count from an existing look up table file.
    map<Uint64, vector<drmp3_seek_point_serial> > seek_points_table;
    map<Uint64, drmp3_uint64> pcm_frame_count_table;
    drmp3_uint64 pcm_frame_count = load_existing_seek_points(seektable_filename,
                                                             stream_hash,
                                                             seek_points_table,
                                                             pcm_frame_count_table,
                                                             p_mp3->seek_points_vector);

    // Otherwise calculate new seek points and save them to the fast-seek file.
    if (pcm_frame_count == 0) {
        pcm_frame_count = generate_new_seek_points(seektable_filename,
                                                   stream_hash,
                                                   p_mp3->p_dr,
                                                   seek_points_table,
                                                   pcm_frame_count_table,
                                                   p_mp3->seek_points_vector);
        if (pcm_frame_count == 0) {
            // LOG_MSG("MP3: could not load existing or generate new seek points for the stream");
            return 0;
        }
    }

    // Finally, regardless of which scenario succeeded above, we now have our seek points!
    // We bind our seek points to the dr_mp3 object which will be used for fast seeking.
    if(drmp3_bind_seek_table(p_mp3->p_dr,
                             static_cast<uint32_t>(p_mp3->seek_points_vector.size()),
                             reinterpret_cast<drmp3_seek_point*>(p_mp3->seek_points_vector.data()))
                             != DRMP3_TRUE) {
        return 0;
    }

    result = true;
    return pcm_frame_count;
}
