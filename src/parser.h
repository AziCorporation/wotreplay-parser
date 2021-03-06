#ifndef wotreplay_parser_h
#define wotreplay_parser_h

#include <boost/filesystem.hpp>
#include <iostream>
#include <set>
#include <vector>

#include "game.h"
#include "packet.h"
#include "packet_reader.h"
#include "types.h"

/** @file */

namespace wotreplay {
    /** wotreplay::parser_t is a class responsible for parsing a World of Tanks replay file.  */
    class parser_t {
        /**
         * @brief Validate the inner workings of the class parser
         * Validate the inner workings of the class parser by:
         * -# checking the number of unread bytes
         * -# validating the parsed packets with the game metadata
         * @param path The directory which contains the replays to validate the parser with
         */
        friend void validate_parser(const std::string &path);
    public:
        /** 
         * Default constructor for creating an empty replay_file 
         */
        parser_t(bool debug = false);
        /**
         * Indicates if the parsed file is a legacy (< 0.7.2) replay file. This means 'game begin', 'game end' will be missing.
         * @return \c true if file is in a legacy format \c false if the file is in the 'new' format.
         */
        bool is_legacy() const;
        /**
         * Set debug mode for this instance.
         * @param debug The new value for debug.
         */
        void set_debug(bool debug);
        /**
         * @return The debug setting for this parser instance.
         */
        bool get_debug() const;
        /**
         * Parses the replay file. The inputstream will be consumed completly.
         * @param is The inputstream containing the replay file.
         * @param game The output variable containing the parsed contents of the replay file.
         */
        void parse(std::istream &is, game_t &game);
        /**
         * Parses the replay file from the contents in the buffer.
         * @param buffer Buffer containing the complete contents of the replay file.
         * @param game The output variable containing the parsed contents of the replay file.
         */
        void parse(buffer_t &buffer, wotreplay::game_t &game);
    private:
        /**
         * Configures parser configuration using the version string.
         * @param version the version string
         * @return Returns if the method was able to configure the parser to match the version
         */
        bool setup(const version_t &version);
        /**
         * Indicates if the passed buffer_t contains a legacy (< 0.7.2) replay file. 
         * @return \c true if file is in a legacy format \c false if the file is in the 'new' format.
         */
        bool is_legacy_replay(const buffer_t &buffer) const;
        /**
         * Extracts each data block from the replay file and adds it the the output variable.
         * @param buffer A buffer_t containing the complete contents of a replay file.
         * @param data_blocks output variable to contain a slice_t to each data block.
         */
        void get_data_blocks(buffer_t &buffer, std::vector<slice_t> &data_blocks) const;
        /**
         * Determines the number of data blcks in the replay file.
         * @param buffer A buffer_t containing the complete contents of a replay file.
         * @return The number of data blocks present in the replay file.
         */
        uint32_t get_data_block_count(const buffer_t &buffer) const;
        /**
         * Performs an in place decryption of the replay using the given key. The decryption
         * is a (broken) variant of CBC decryption and is performed as follows:
         * -# Set the variable previous_block (with size of 16 bytes) to 0
         * -# Decrypt a block with the given key
         * -# XOR the block with the previous (decrypted) block
         * -# Go back to step 2, until there are no more blocks.
         * @param buffer A buffer_t containing the contents of the data block 'replay'.
         * @param key The blowfish key used for decryption.
         */
        void decrypt_replay(buffer_t &buffer, const unsigned char* key);
        /**
         * This method performs an extraction (using zlib) of the given replay. The resulting
         * data is copied to a new buffer.
         * @param compressed_replay The buffer containing the compressed replay
         * @param replay The output variable that will contain the decompressed replay after the execution of this method.
         */
        void extract_replay(buffer_t &compressed_replay, buffer_t &replay);
        /**
         * Helper method to read a single packet from a begin and end iterator.
         * @param begin iterator to denote the beginning of the packet.
         * @param end iterator to denote the end of the packet.
         * @return The newly constructed packet.
         */
        template <typename iterator>
        packet_t read_packet(iterator begin, iterator end);
        /**
         * Helper method to read all the packets contained in the buffer replay_file::replay.
         * @param game Output variable game
         */
        void read_packets(game_t &game);
        /**
         * Extracts a game_info object from either the 'game begin' data block or parsed directly from the 'replay'
         * data block.
         * @param game Output variable game
         */
        void read_game_info(game_t &game);
        /** The legacy indicator. */
        bool legacy;
        /** The debug indicator */
        bool debug;
        /** Packet lengths */
        std::map<uint8_t, int> packet_lengths;
        std::unique_ptr<packet_reader_t> packet_reader;
    };

    template <typename iterator>
    packet_t parser_t::read_packet(iterator begin, iterator end) {
        return packet_t(boost::make_iterator_range(begin, end));
    }

    /**
     * Shows a quick overview of the packet count in the replay file.
     * @param packets A list of packets parsed from a replay file.
     */
    void show_packet_summary(const std::vector<packet_t>& packets);

    /**
     * @fn void validate_parser(const std::string &path)
     * @brief Validate the parser to multiple replay files stored in the directory
     * @param path The path of the directory containing replay files
     */
    void validate_parser(const std::string &path);

    /**
     * @fn bool is_replayfile(const boost::filesystem::path &p);
     * @brief Basic function to check if a path might be a replay file.
     * @param p The path of the file to inspect for being a replay file.
     * @return @c true if the path is expected to be a valid replay file, @c false if not
     */
    bool is_replayfile(const boost::filesystem::path &p);
}

#endif /* defined(wotreplay_parser_h) */
