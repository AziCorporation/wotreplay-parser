#ifndef wotreplay_logger_h
#define wotreplay_logger_h

#include <iosfwd>
#include <string>


namespace wotreplay {
    enum log_level_t {
        error,
        warning,
        info,
        debug
    };

    /**
     * basic logger class
     */
    class logger_t {
    public:
        /**
         * Create new instance of logger, logger will write to the given output stream
         * @param os target outputstream
         */
        logger_t(std::ostream& os);
        /**
         * Define the minimum log level
         * @param level new minimum log level
         */
        void set_log_level(log_level_t level);
        /**
         * Write a message to the log file
         * @param level define a level for the message
         * @param message the message to log
         */
        void write(log_level_t level, const std::string& message);
    private:
        std::ostream &os;
        log_level_t level = log_level_t::warning;
    };

    extern logger_t log;
}

#endif /* defined(wotreplay_logger_h) */
