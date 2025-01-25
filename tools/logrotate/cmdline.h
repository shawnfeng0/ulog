/** @file cmdline.h
 *  @brief The header file for the command line option parser
 *  generated by GNU Gengetopt version 2.23
 *  http://www.gnu.org/software/gengetopt.
 *  DO NOT modify this file, since it can be overwritten
 *  @author GNU Gengetopt */

#ifndef CMDLINE_H
#define CMDLINE_H

/* If we use autoconf.  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h> /* for FILE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CMDLINE_PARSER_PACKAGE
/** @brief the program name (used for printing errors) */
#define CMDLINE_PARSER_PACKAGE "logrotate"
#endif

#ifndef CMDLINE_PARSER_PACKAGE_NAME
/** @brief the complete program name (used for help and version) */
#define CMDLINE_PARSER_PACKAGE_NAME "logrotate"
#endif

#ifndef CMDLINE_PARSER_VERSION
/** @brief the program version */
#define CMDLINE_PARSER_VERSION "0.6.0"
#endif

/** @brief Where the command line options are stored */
struct gengetopt_args_info
{
  const char *help_help; /**< @brief Print help and exit help description.  */
  const char *version_help; /**< @brief Print version and exit help description.  */
  char * file_path_arg;	/**< @brief File path to record log.  */
  char * file_path_orig;	/**< @brief File path to record log original value given at command line.  */
  const char *file_path_help; /**< @brief File path to record log help description.  */
  char * file_size_arg;	/**< @brief Size of each file (default='1MB').  */
  char * file_size_orig;	/**< @brief Size of each file original value given at command line.  */
  const char *file_size_help; /**< @brief Size of each file help description.  */
  int max_files_arg;	/**< @brief Maximum number of files (default='8').  */
  char * max_files_orig;	/**< @brief Maximum number of files original value given at command line.  */
  const char *max_files_help; /**< @brief Maximum number of files help description.  */
  char * flush_interval_arg;	/**< @brief Time interval between flushing to disk (default='1s').  */
  char * flush_interval_orig;	/**< @brief Time interval between flushing to disk original value given at command line.  */
  const char *flush_interval_help; /**< @brief Time interval between flushing to disk help description.  */
  char * rotation_strategy_arg;	/**< @brief File rotation strategy:
  rename: log.1.txt -> log.2.txt
  incremental: log-24.txt ... log-34.txt (default='rename').  */
  char * rotation_strategy_orig;	/**< @brief File rotation strategy:
  rename: log.1.txt -> log.2.txt
  incremental: log-24.txt ... log-34.txt original value given at command line.  */
  const char *rotation_strategy_help; /**< @brief File rotation strategy:
  rename: log.1.txt -> log.2.txt
  incremental: log-24.txt ... log-34.txt help description.  */
  int rotate_first_flag;	/**< @brief Should rotate first before write (default=off).  */
  const char *rotate_first_help; /**< @brief Should rotate first before write help description.  */
  char * fifo_size_arg;	/**< @brief Size of the FIFO buffer (default='32KB').  */
  char * fifo_size_orig;	/**< @brief Size of the FIFO buffer original value given at command line.  */
  const char *fifo_size_help; /**< @brief Size of the FIFO buffer help description.  */
  int zstd_compress_flag;	/**< @brief Compress with zstd (default=off).  */
  const char *zstd_compress_help; /**< @brief Compress with zstd help description.  */
  char * zstd_params_arg;	/**< @brief Parameters for zstd compression,
  larger == more compression and memory (e.g., level=3,window-log=21,chain-log=16,hash-log=17).  */
  char * zstd_params_orig;	/**< @brief Parameters for zstd compression,
  larger == more compression and memory (e.g., level=3,window-log=21,chain-log=16,hash-log=17) original value given at command line.  */
  const char *zstd_params_help; /**< @brief Parameters for zstd compression,
  larger == more compression and memory (e.g., level=3,window-log=21,chain-log=16,hash-log=17) help description.  */
  
  unsigned int help_given ;	/**< @brief Whether help was given.  */
  unsigned int version_given ;	/**< @brief Whether version was given.  */
  unsigned int file_path_given ;	/**< @brief Whether file-path was given.  */
  unsigned int file_size_given ;	/**< @brief Whether file-size was given.  */
  unsigned int max_files_given ;	/**< @brief Whether max-files was given.  */
  unsigned int flush_interval_given ;	/**< @brief Whether flush-interval was given.  */
  unsigned int rotation_strategy_given ;	/**< @brief Whether rotation-strategy was given.  */
  unsigned int rotate_first_given ;	/**< @brief Whether rotate-first was given.  */
  unsigned int fifo_size_given ;	/**< @brief Whether fifo-size was given.  */
  unsigned int zstd_compress_given ;	/**< @brief Whether zstd-compress was given.  */
  unsigned int zstd_params_given ;	/**< @brief Whether zstd-params was given.  */

} ;

/** @brief The additional parameters to pass to parser functions */
struct cmdline_parser_params
{
  int override; /**< @brief whether to override possibly already present options (default 0) */
  int initialize; /**< @brief whether to initialize the option structure gengetopt_args_info (default 1) */
  int check_required; /**< @brief whether to check that all required options were provided (default 1) */
  int check_ambiguity; /**< @brief whether to check for options already specified in the option structure gengetopt_args_info (default 0) */
  int print_errors; /**< @brief whether getopt_long should print an error message for a bad option (default 1) */
} ;

/** @brief the purpose string of the program */
extern const char *gengetopt_args_info_purpose;
/** @brief the usage string of the program */
extern const char *gengetopt_args_info_usage;
/** @brief the description string of the program */
extern const char *gengetopt_args_info_description;
/** @brief all the lines making the help output */
extern const char *gengetopt_args_info_help[];

/**
 * The command line parser
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser (int argc, char **argv,
  struct gengetopt_args_info *args_info);

/**
 * The command line parser (version with additional parameters - deprecated)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param override whether to override possibly already present options
 * @param initialize whether to initialize the option structure my_args_info
 * @param check_required whether to check that all required options were provided
 * @return 0 if everything went fine, NON 0 if an error took place
 * @deprecated use cmdline_parser_ext() instead
 */
int cmdline_parser2 (int argc, char **argv,
  struct gengetopt_args_info *args_info,
  int override, int initialize, int check_required);

/**
 * The command line parser (version with additional parameters)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param params additional parameters for the parser
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_ext (int argc, char **argv,
  struct gengetopt_args_info *args_info,
  struct cmdline_parser_params *params);

/**
 * Save the contents of the option struct into an already open FILE stream.
 * @param outfile the stream where to dump options
 * @param args_info the option struct to dump
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_dump(FILE *outfile,
  struct gengetopt_args_info *args_info);

/**
 * Save the contents of the option struct into a (text) file.
 * This file can be read by the config file parser (if generated by gengetopt)
 * @param filename the file where to save
 * @param args_info the option struct to save
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int cmdline_parser_file_save(const char *filename,
  struct gengetopt_args_info *args_info);

/**
 * Print the help
 */
void cmdline_parser_print_help(void);
/**
 * Print the version
 */
void cmdline_parser_print_version(void);

/**
 * Initializes all the fields a cmdline_parser_params structure 
 * to their default values
 * @param params the structure to initialize
 */
void cmdline_parser_params_init(struct cmdline_parser_params *params);

/**
 * Allocates dynamically a cmdline_parser_params structure and initializes
 * all its fields to their default values
 * @return the created and initialized cmdline_parser_params structure
 */
struct cmdline_parser_params *cmdline_parser_params_create(void);

/**
 * Initializes the passed gengetopt_args_info structure's fields
 * (also set default values for options that have a default)
 * @param args_info the structure to initialize
 */
void cmdline_parser_init (struct gengetopt_args_info *args_info);
/**
 * Deallocates the string fields of the gengetopt_args_info structure
 * (but does not deallocate the structure itself)
 * @param args_info the structure to deallocate
 */
void cmdline_parser_free (struct gengetopt_args_info *args_info);

/**
 * Checks that all the required options were specified
 * @param args_info the structure to check
 * @param prog_name the name of the program that will be used to print
 *   possible errors
 * @return
 */
int cmdline_parser_required (struct gengetopt_args_info *args_info,
  const char *prog_name);

extern const char *cmdline_parser_rotation_strategy_values[];  /**< @brief Possible values for rotation-strategy. */


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CMDLINE_H */
