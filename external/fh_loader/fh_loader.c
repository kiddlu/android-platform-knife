/****************************************************************************
 *
 * Speaks Qualcomm FIREHOSE protocol.
 *
 * Release Version: 16.02.03.12.31.25 (Feb  3 2016 @ 12:31:25)
 *
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 ****************************************************************************

=============================================================================

                        EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

 $Header: //source/qcom/qct/core/storage/tools/fh_loader/vs2010/fh_loader/fh_loader.c#16 $
 $DateTime: 2016/02/03 13:05:34 $
 $Author: wkimberl $

when        who  what, where, why
----------  ---  ---------------------------------------------------------
2016-02-03  wek  Return a non zero value on error.
2016-01-29  wek  Skip over files that have ignore to true on contents.xml
2016-01-28  wek  Fix the number of hashes of the first VIP table to 54.
2016-01-27  wek  Increase the maximum size of the input XML file
2016-01-27  wek  Start sector from the end of the device using negative numbers
2016-01-27  wek  Report error on NAK for unrecognized command.
2016-01-14  wek  Move all the SHA functions to a spearate file.
2015-12-02  wek  Remove strtoull, not supported on VS2010.
2015-09-29  wek  Increase the max number of Commands/XMLs that can be sent.
2015-08-18  ky   flavor no longer necessary in contentsxml
2015-08-13  ky   Showpercentagecomplete works with flattenbuild
2015-08-10  ky   Don't copy over existing flattenbuild files unless forceoverwrite
2015-08-07  ky   Redirect all output files to MainOutputDir option
2015-08-06  ky   Added large file support
2015-07-21  ky   Added windows FFU image support.
2015-07-02  wek  Fix error of opening NULL file for sparse files.
2015-07-02  wek  Removing uncessesary memset.
2015-06-18  ky   Added sparse image support.
2015-06-03  wek  Fix printing wrong build size for large builds.
2015-06-02  wek  Accept upper case UFS from the command line argument.
2015-06-02  wek  Handle VIP tables that are multiple of 512 bytes.
2015-05-29  wek  Fix printing wrong percentage complete on large builds.
2015-05-27  wek  Fix the sector size for firmware update from the command line.
2015-05-26  ah   Do wipe first only for program tag.
2015-05-20  wek  Fix firmware update from the command line.
2015-05-20  wek  Fix double printing entries in the output digest file.
2015-05-15  wek  Print version, the version is same as date.
2015-05-15  wek  Print out the percentage when reading from the device.
2015-05-14  wek  Copy the device programmer binary when flattening build.
2015-05-06  wek  Change how the percentage completed is printed.
2015-05-05  wek  Move populating variable after the log name is known.
2015-05-04  wek  Run the soruce through a beautifier.
2015-04-22  ah   Build flattening improved
2015-04-12  ah   Build flattening based on flavor added
2015-03-27  ah   COM port hanging is corrected
2015-03-27  ah   Various features added
2015-03-24  ah   Various features added
2015-03-06  ah   Initial release

=============================================================================

O: maps to \\corebsp-tst-173\c$\preflight\builds\test_job_1\8994

Get driver working on Linux
sudo rmmod qcserial
sudo rmmod usbserial
sudo modprobe usbserial vendor=0x5c6 product=0x9008

After this
ahughes@ahughes-laptop-dell:~/programming/fh_loader$ ls /dev/ttyU*
/dev/ttyUSB0

Get DeviceProgrammer file over to Kickstart
cd programming/kickstart_Feb10_2015
cp /media/PENDRIVE/prog_emmc_firehose_8994_ddr.mbn .
./kickstart -p /dev/ttyUSB0 -s 13:prog_emmc_firehose_8994_ddr.mbn    <-- NOTE: kickstart=QSaharaServer.exe in Windows

gcc fh_loader.c -o fh_loader -lrt

./fh_loader --port=/dev/ttyUSB0 --sendimage=big.bin --noreset --verify_programming

****************************************************************************/

#ifdef _MSC_VER // i.e. if compiling under Windows
#include "stdafx.h"
#include <windows.h>
#include <direct.h>
#include <io.h>
#define GETCWD _getcwd
#define ZLPAWAREHOST 1
#define SLASH '\\'    // defined differently below for LINUX
#define WRONGSLASH '/'
#define O_RDWR _O_RDWR
#define O_SYNC _O_SEQUENTIAL
#define sleep(x) Sleep(x*1000)

#define SIZE_T long long int
#define SIZE_T_FORMAT "lld"    // Use in middle of string "Channel read "SIZE_T_FORMAT" bytes", at end of string "num_physical_partitions="SIZE_T_FORMAT
#define SIZE_T_FORMAT4 ".4lld"
#define SIZE_T_FORMAT5 ".5lld"
#define fseek _fseeki64
#define ftell _ftelli64

#else
#include <termios.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <errno.h>
#define ZLPAWAREHOST 0
#define SLASH '/'
#define WRONGSLASH '\\'
#define GETCWD getcwd
#define SIZE_T_FORMAT "lld"   // Use in middle of string "Channel read "SIZE_T_FORMAT" bytes", at end of string "num_physical_partitions="SIZE_T_FORMAT
#define SIZE_T_FORMAT4 ".4lld"
#define SIZE_T_FORMAT5 ".5lld"
#define _stat64 stat

typedef long long int SIZE_T;

#endif

#define BACKSLASH 92
#define FORWARDSLASH 47
#define COMMAND_TRACE_BYTES_TO_RECORD 128

#include <sys/types.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fh_loader_sha.h"

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#define SPARSE_HEADER_MAGIC 0xed26ff3a
#define CHUNK_TYPE_RAW 0xCAC1
#define CHUNK_TYPE_FILL 0xCAC2
#define CHUNK_TYPE_DONT_CARE 0xCAC3
#define SPARSE_HEADER_MAJOR_VER 1

#define ONE_MEGABYTE  1048576
#define MAX_STRING_SIZE 2048
#define MAX_PATH_SIZE 2048
#define MAX_XML_SIZE    (64*1024)
#define MAX_STRING_PAIR_SIZE (2*4096)

#define MAX(a,b) (((a)>(b))?(a):(b))

char *CommandLineOptions[] = {  "benchmarkddr", "benchmarkdigestperformance", "benchmarkreads", "benchmarkwrites",
                                "createcommandtrace", "createdigests", "chaineddigests=", "comportopentimeout=",
                                "contentsxml=", "convertprogram2read", "createconfigxml", "digestsperfilename=",
                                "dontsorttags",
                                "erase=", "erasefirst", "files=", "firmwarewrite", "flattenbuildto=", "flavor=", "forcecontentsxmlpaths", "forceoverwrite",
                                "getstorageinfo=", "getgptmainbackup=",
                                "interactive=", "loglevel=", "lun=", "mainoutputdir=", "maxdigesttablesizeinbytes=", "maxpayloadsizeinbytes=", "memoryname=",
                                "notfiles=", "nop", "noprompt", "noreset", "num_sectors=", "port=", "porttracename=",
                                "readbogusdata", "reset",
                                "search_path=", "sectorsizeinbytes=", "sendimage=", "sendxml=", "setactivepartition=",
                                "showdigestperpacket", "showdigestperfile", "showpercentagecomplete", "signeddigests=", "simulate", "skipharddriveread",
                                "skippatch", "skipwrite", "skipstorageinit", "start_sector=", "stresstest", "testvipimpact", "trials=",
                                "wipefirst", "verbose", "verify_build", "verify_programming",
                                "zlpawarehost="
                             };

long long unsigned COPY_BUF_SIZE = (4UL * 1024 * 1024 * 1024) - 1; //4gb max copy buf size

typedef unsigned char boolean;
typedef unsigned char byte;


#define XML_HEADER_LENGTH 41    // 40 does NOT include null
#define XML_TAIL_LENGTH   9     // 9 DOES include null

/*-----------------------------------------
*
*    FFU Related Structs
*
------------------------------------------*/
enum DISK_ACCESS_METHOD
{
    DISK_BEGIN = 0,
    DISK_SEQ = 1,
    DISK_END = 2
};
//header structs for ffu images
struct gpt_entry_t
{
    char        type_guid[16];
    char        unique_guid[16];
    uint64_t    first_lba;
    uint64_t    last_lba;
    uint64_t    attributes;
    char        part_name[72];
};

struct gpt_header_t
{
    char        signature[8];
    int32_t     revision;
    int32_t     header_size;
    int32_t     crc_header;
    int32_t     reserved;
    uint64_t    current_lba;
    uint64_t    backup_lba;
    uint64_t    first_lba;
    uint64_t    last_lba;
    char        disk_guid[16];
    uint64_t    partition_lba;
    int32_t     num_entries;
    int32_t     entry_size;
    int32_t     crc_partition;
    char        reserved2[420];
};

struct FFUSecurityHeader_t
{
    uint32_t    cbSize; //size of struct, overall
    char        signature[12]; //"Signed Image"
    uint32_t    dwChunkSizeInKb; //Size of hashed chunk within the image
    uint32_t    dwAlgId;        //algorithm used to hash
    uint32_t    dwCatalogSize; //size of catalog to validate
    uint32_t    dwHashTableSize;    //size of hash table
};

struct FFUImageHeader_t
{
    uint32_t    cbSize; //size of imageheader
    char        signature[12]; //"ImageFlash "
    uint32_t    ManifestLength; // in bytes
    uint32_t    dwChunkSize;    // Used only during image generation
};

struct FFUStoreHeader_t
{
    uint32_t    dwUpdateType; // indicates partial or full flash
    uint16_t    MajorVersion; //used to validate struct
    uint16_t    MinorVersion; //used to validate struct
    uint16_t    FullFlashMajorVersion;
    uint16_t    FullFlashMinorVersion; //FFU version, i.e. the image format
    char        szPlatformId[192]; //string which indicates what device this FFU is inteded to be written to
    uint32_t    dwBlockSizeInBytes; //size of an image block in bytes - actual sector size may differ
    uint32_t    dwWriteDescriptorCount; //number of write descriptors to iterate through
    uint32_t    dwWriteDescriptorLength; //total size of all the write descriptors in bytes.
    uint32_t    dwValidateDescriptorCount; //number of validation descriptors to iterate through
    uint32_t    dwValidateDescriptorLength; //total size of all the validation descriptors in bytes
    uint32_t    dwInitialTableIndex; //block index in the payload of the initial GPT
    uint32_t    dwInitialTableCount; // count of blocks in the flash-only GPT
    uint32_t    dwFlashOnlyTableIndex; //first block index in the payload of the flash-only GPT
    uint32_t    dwFlashOnlyTableCount; //coutn of blocks in the flash-only GPT
    uint32_t    dwFinalTableIndex; //index in the table of the real GPT
    uint32_t    dwFinalTableCount; //number of blocks in the real GPT
};

struct FFUDiskLocation_t
{
    uint32_t    dwDiskAccessMethod;
    uint32_t    dwBlockIndex;
};

struct FFUBlockDataEntry_t
{
    uint32_t                    dwLocationCount;
    uint32_t                    dwBlockCount;
    struct FFUDiskLocation_t    rgDiskLocations[1];
};

struct FFUImage_t
{
    //Info related to flashing ffu image
    char                            filename[MAX_STRING_SIZE];
    SIZE_T                          sector_size_in_bytes;
    SIZE_T                          physical_partition_number;
    //header info
    struct FFUSecurityHeader_t      FFUSecurityHeader;
    struct FFUImageHeader_t         FFUImageHeader;
    struct FFUStoreHeader_t         FFUStoreHeader;
    char *                          ValidationEntries;
    struct FFUBlockDataEntry_t *    BlockDataEntries;
    uint64_t                        PayloadDataStart;

    //GPT Stuff
    char                            GptProtectiveMBR[512];
    struct gpt_header_t             GptHeader;
    struct gpt_entry_t *            GptEntries;
};

struct sparse_handle_t
{
    FILE *                  fd;
    uint32_t                total_groups;
    uint16_t                chunk_hdr_sz;
    //array for chunks of raw data
    unsigned long long *    start_sector;
    unsigned long long *    bytes_remaining; //how many bytes left in group
    long long int *         group_file_offset; //file position of a chunk header
    int                     chunk_header_idx; //index into above array
};

struct sparse_header_t
{
    uint32_t    magic;      /* 0xed26ff3a */
    uint16_t    major_version;  /* (0x1) - reject images with higher major versions */
    uint16_t    minor_version;  /* (0x0) - allow images with higer minor versions */
    uint16_t    file_hdr_sz;    /* 28 bytes for first revision of the file format */
    uint16_t    chunk_hdr_sz;   /* 12 bytes for first revision of the file format */
    uint32_t    blk_sz;     /* block size in bytes, must be a multiple of 4 (4096) */
    uint32_t    total_blks; /* total blocks in the non-sparse output imasector_size_in_bytesge */
    uint32_t    total_chunks;   /* total chunks in the sparse input image */
    uint32_t    image_checksum; /* CRC32 checksum of the original data, counting "don't care" */
    /* as 0. Standard 802.3 polynomial, use a Public Domain */
    /* table implementation */
};

struct chunk_header_t
{
    uint16_t    chunk_type; /* 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care */
    uint16_t    reserved1;
    uint32_t    chunk_sz;   /* in blocks in output image */
    uint32_t    total_sz;   /* in bytes of chunk input file including chunk header and data */
};

struct sparse_read_info_t
{
    unsigned long long  chunk_remaining;
    unsigned long long  bytes_remaining;
};

static const char xml_header[XML_HEADER_LENGTH] = "<\?xml version=\"1.0\" encoding=\"UTF-8\" \?>\n";

char ShowXMLFileInLog = 0;  // Controls wether DetermineTag shows the XML file, because I want to show HOST to TARGET

/* List of variables provided for documentation*/
/* xml_tail[XML_TAIL_LENGTH]     = "\n</data>"; */
/* OPENING_DATA_TAG[] = "<data>"; */
/* CLOSING_DATA_TAG[] = "</data>"; */
/* CLOSING_CONFIGURATION_TAG[] = "</configuration>"; */
/* OPENING_PATCHES_TAG[] = "<patches>"; */
/* CLOSING_PATCHES_TAG[] = "</patches>"; */
/* CLOSING_TAG[] = "/>"; */

char LOAD_RAW_PROGRAM_FILES  = 1;
char LOAD_PATCH_PROGRAM_FILES = 1;

char UsingValidation = 0; // flag that controls VIP, i.e. if we send Digest Tables or not
char ShowDigestPerPacket = 0, createcommandtrace = 0, showpercentagecomplete = 0, ShowDigestPerFile = 0;
char ConvertProgram2Firmware = 0, ConvertProgram2Read = 0, forcecontentsxmlpaths = 0, verify_build = 0;

char ThereWereErrors = 0;

char RemoveCommentsFromXMLFiles = 1;
char ParseAttributes = 1; // this changes from 0 to 1 during the program

float ComPortOpenTimeout = 3.0;

char cwd[MAX_PATH_SIZE];
char MainOutputDir[MAX_PATH_SIZE] = {0};
char SignedDigestTableFilename[MAX_STRING_SIZE];
char ChainedDigestTableFilename[MAX_STRING_SIZE];
char ContentsXMLFilename[MAX_STRING_SIZE];
char flattenbuildto[MAX_STRING_SIZE];
char flavor[MAX_STRING_SIZE];
char flattenbuildvariant[MAX_STRING_SIZE];  // such as eMMC, UFS etc

long DeviceProgrammerIndex = -1;

char MyArg[MAX_STRING_SIZE], MyOpt[MAX_STRING_SIZE]; // Command line args
char MyStringPairs[MAX_STRING_PAIR_SIZE];
#define MAX_TAG_NAME_LENGTH 128

SIZE_T ThisXMLLength; // Updated by RemoveEverythingButTags()
unsigned long long TotalTransferSize = 0, BuildSizeTransferSoFar = 0; // sum up all the transfers (reads and writes)
float  PercentageBuildLoaded = 0.0;

SIZE_T CopyString (char *Dest, char *Source, SIZE_T Dstart, SIZE_T  Sstart, SIZE_T  length, SIZE_T DestSize, SIZE_T SourceSize);
boolean MyParseExpression (char* buffer, SIZE_T BufferLength, SIZE_T* result);
SIZE_T  ParseComplicatedAttributes (void);

struct timeval fh_loader_start, fh_loader_end;

int MyCopyFile (char *FileNameSource, char *FileNameDest);
SIZE_T stringToNumber (const char* buffer, boolean *retval);
void CleanseSearchPaths (void);
void ModifyTags (void);
void LoadConfigureIntoTXBuffer (void);
void PossiblyShowContentsXMLDifferentFileFoundWarning (char *CurrentPathAndFilenameFound);
void CheckContentsXMLCompleteFileAndPath (char *filename); // fills contents_full_filename_with_path
void ParseContentsXML (char *FileAndPath);
void FindPartitionByLabel (SIZE_T LUN, char *LabelToMatch, char *Filename);

void ReturnSizeString (unsigned long long size, char *sz, unsigned long long Length);
int IsASCIIString (char *sz);

void PrepareDigestTable (void);

void ShowCommandLineOptions (void);
int PartOfCommandLineOptions (char *sz);

SIZE_T NumTries = 1000;

//firehose_error_t GetNextPacket(void);

FILE * ReturnFileHandle (char *filename, long MaxLength, char *AccessMode);

int AlreadyHaveThisFile (char *full_filename_with_path);
int AlreadyHaveThisPath (char *path);
int ThisFileIsInFilterFiles (char *filename_only);
int ThisFileIsInNotFilterFiles (char *filename_only);

int HasAPathCharacter (char *sz, SIZE_T Length);
int IsARelativePath (char *sz, SIZE_T Length);


void PrettyPrintHexValueIntoTempBuffer (uint8 *temp_hash_val, int length, int offset, int MaxLength);

void TrimTrailingWhiteSpaceFromStringVariable (char *sz);

void SendSignedDigestTable (char *SignedDigestTableFilename);
void TestIfWeNeedToSendDigestTable (void);

void parseConfigXML (SIZE_T XMLFileSize);
void ExitAndShowLog (unsigned int ExitCode);

void display_error (FILE *MyFP);
void display_warning (FILE *MyFP);

char ConfigXML[MAX_PATH_SIZE];    // used for --xml=firehose_config.xml
char port_name[MAX_STRING_SIZE];
SIZE_T SectorSizeInBytes = 512;   // fh.attrs.SECTOR_SIZE_IN_BYTES updated on every RawProgram.xml file, so compare to this
char GotACK = 0, stresstest = 0;
char AllowReset = 0; // user must specify --reset now
char PromptUser  = 1, SortTags = 1;

#define MAX_XML_FILES 2000
char XMLFileTable[MAX_XML_FILES][MAX_PATH_SIZE];    // --sendxml=command1.xml,command2.xml,etc
char search_path[MAX_XML_FILES][MAX_PATH_SIZE];     // --search_path=command1.xml,command2.xml,etc
char filter_files[MAX_XML_FILES][MAX_PATH_SIZE];    // --files=sbl1.mbn,tz.mbn
char filter_not_files[MAX_XML_FILES][MAX_PATH_SIZE];// --notfiles=sbl1.mbn,tz.mbn
char XMLStringTable[MAX_XML_FILES][MAX_XML_SIZE];   // --sendxml=command1.xml,command2.xml,etc
char XMLStringTableTemp[MAX_XML_FILES][MAX_XML_SIZE]; // --sendxml=command1.xml,command2.xml,etc
SIZE_T NumXMLFilesInTable = 1;                      // start at 1 since <configure> will go at 0

#define MAX_FILES_TO_TRACK 300
char MaxFilesToTrack[MAX_FILES_TO_TRACK][MAX_PATH_SIZE];    // --search_path=command1.xml,command2.xml,etc
SIZE_T FileToTrackCount = 0;

#define MAX_XML_FILE_SIZE (128*1024)
char EntireXMLFileBuffer[MAX_XML_FILE_SIZE];

char PortTraceName[MAX_PATH_SIZE]      = "port_trace.txt";
char DigestsPerFileName[MAX_PATH_SIZE] = "DigestsPerFile.txt";
char CommandTraceName[MAX_PATH_SIZE]   = "command_trace.txt";
char ConfigXMLName[MAX_PATH_SIZE]      = "config_";

static char full_filename_with_path[MAX_PATH_SIZE];
static char contents_full_filename_with_path[MAX_PATH_SIZE];
SIZE_T num_search_paths = 0, num_xml_files_to_send = 0, num_filter_files = 0, num_filter_not_files = 0;


FILE * fp = NULL; // for port_trace.txt
FILE * fc = NULL; // for command_trace.txt
FILE * ft = NULL; // for DIGEST_TABLE.bin
FILE * fg = NULL; // for Config.xml from --createconfigxml
FILE * fdp = NULL; // for DigestsPerFile.txt

char RawMode = 0; // 0 means XML files from target, 1 means RAW packets as in a <read> command
char testvipimpact = 0;

#define FIREHOSE_TX_BUFFER_SIZE (ONE_MEGABYTE)
char tx_buffer[FIREHOSE_TX_BUFFER_SIZE];
char tx_buffer_backup[FIREHOSE_TX_BUFFER_SIZE];
char temp_buffer[FIREHOSE_TX_BUFFER_SIZE];
char temp_buffer2[FIREHOSE_TX_BUFFER_SIZE];
char temp_buffer3[FIREHOSE_TX_BUFFER_SIZE];
char last_log_value[FIREHOSE_TX_BUFFER_SIZE];

#define PORT_TRACE_LOG_BUFFER_SIZE (ONE_MEGABYTE)       // 1MB buffered to file
//#define PORT_TRACE_LOG_BUFFER_SIZE (50)       // 1MB buffered to file

//char PortTraceLogBuffer[PORT_TRACE_LOG_BUFFER_SIZE];
//SIZE_T PortTraceLogBufferStart = 0, PortTraceLogBufferEnd=0;

//char tx_buffer2[FIREHOSE_TX_BUFFER_SIZE+FIREHOSE_TX_BUFFER_SIZE];


enum LOG_TYPES
{
    LOG_ERROR = 1,
    LOG_ALWAYS,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,  // usually only goes to log, unless VerboseLevel==LOG_DEBUG
    LOG_ONLY  // always only goes to log, such as hex dump
};



typedef enum firehose_error_t
{
    FIREHOSE_SUCCESS,
    FIREHOSE_ERROR,
    FIREHOSE_TRANSPORT_ERROR,
    FIREHOSE_STORAGE_ERROR,
    FIREHOSE_VALIDATION_ERROR
} firehose_error_t;

typedef firehose_error_t (*tag_handler_t) ();

typedef struct
{
    char tag_name[32];      // configure
    tag_handler_t handler;  // handleConfigure()
} firehose_tag_data_t;

tag_handler_t CurrentHandlerFunction;   // this will always point to latest TAG function, i.e. handleConfigure()

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#define FIREHOSE_TEMP_BUFFER_SIZE 512

SIZE_T BytesWritten = 0;
SIZE_T BytesRead = 0; // How much USB data we read


SIZE_T PacketsSent            = 0;
SIZE_T CurrentDigestLocation  = 0;
SIZE_T digest_file_offset     = 0;
SIZE_T NumDigestsFound        = 0;
SIZE_T DigestSizeInBytes      = 32;
SIZE_T MaxNumDigestsPerTable  = 0;

char verify_programming   = 0;

uint8 temp_hash_value[32] = {0};
uint8 verify_hash_value[32] = {0};
uint8 last_hash_value[32] = {0};
SIZE_T SizeOfDataFedToHashRoutine = 0;

struct __sechsh_ctx_s   context;
struct __sechsh_ctx_s   context_per_packet;

int ReturnAttributeLocation (char *NewAttrName);
SIZE_T ReturnFileSize (FILE *fd);
void print_usage (FILE *stream);
char* find_file (char *filename, char ShowToScreen);
void SendXMLString (char *sz, SIZE_T MaxLength);
void StoreXMLFile (char *FileWithPath);

SIZE_T SplitStringOnCommas (char *sz, SIZE_T SizeOfString, char szArray[][MAX_PATH_SIZE], int offset, SIZE_T ArraySize, SIZE_T StringSize);

double ReturnTimeInSeconds (struct timeval *start_time, struct timeval *end_time);

int gettimeofday (struct timeval *t, void* tzp);
void time_throughput_calculate (struct timeval *start_time, struct timeval *end_time, size_t size_bytes, double NetworkElapsed, enum LOG_TYPES ThisLogType);
static int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);

SIZE_T  IsEmptySpace (char c);

firehose_error_t GetNextPacket (void);
static firehose_error_t handleProgram();
static firehose_error_t handleBenchmark();
static firehose_error_t handleFirmwareWrite();
static firehose_error_t handleRead();
static firehose_error_t handleConfigure();
static firehose_error_t handlePower();
static firehose_error_t handleSetBootableStorageDrive();
static firehose_error_t handleLog();
static firehose_error_t handleNop();
static firehose_error_t handleErase();
static firehose_error_t handleUnrecognized();
static firehose_error_t handleOptions();
static firehose_error_t handleFileMappings();
static firehose_error_t handleSearchPaths();
static firehose_error_t handleRawProgram();
static firehose_error_t handlePatchProgram();

static firehose_error_t handleResponse();
void InitBufferWithXMLHeader (char *MyBuffer, SIZE_T Length);
void AppendToBuffer (char *MyBuffer, char *buf, SIZE_T MaxBufferSize);
unsigned int sendTransmitBuffer (void);
unsigned int sendTransmitBufferBytes (SIZE_T Length);
//void SendReset(void);
//void SendConfigure(void);
void LoadConfigureIntoStringTable (void);
void LoadResetIntoStringTable (void);
void SendXmlFiles (void);
void SortMyXmlFiles (void);
void OpenAndStoreAllXMLFiles (void);

/* ============================================================================
**  Function : sparse_open
** ============================================================================
*/
/**
@brief
Initializes data structure for handling a sparse file

@detail
Determines the groups and where different groups are split in sparse image.

@param[in]: fd                          File pointer associated with the sparse file
@param[in]: current_start_sector        The start sector as indicated in the XML file
@param[in]: sector_size_in_bytes        Sector size as indicated in the XML file

@dependencies
fd must be at the start of the file

@sideeffects
None

@return
struct sparse_handle_t pointer with the relevant information from the sparse file

*/
struct sparse_handle_t * sparse_open (FILE * fd, uint64_t current_start_sector, uint64_t sector_size_in_bytes);

/* ============================================================================
**  Function : sparse_read
** ============================================================================
*/
/**
@brief
Reads up to num_bytes bytes from the sparse file.

@detail
Reads up to min(num_bytes, bytes remaining in chunk) bytes from file skipping over all chunk headers.

@param[in]: data                        Buffer to be read into
@param[in]: num_bytes                   Maximum amount of bytes to be read in (could return less)
@param[in]: fd                          File pointer associated with sparse image
@param[in]: sparse_read_struct          File handler from sparse_open associated with sparse image. Modified by function
@param[in]: sparse_header               Header information from the top of the sparse image

@dependencies
sparse_open and read_sparse_header must be called before this. Must also call read_chunk_header once.

@sideeffects
None

@return
number of bytes read. sparse_read_struct is modified.

*/
size_t sparse_read (char * data, size_t num_bytes, FILE * fd, struct sparse_read_info_t * sparse_read_struct, struct sparse_header_t sparse_header);

/* ============================================================================
**  Function : sparse_close
** ============================================================================
*/
/**
@brief
Terminates the structure associated with the sparse image

@detail
Free all handlers associated with sparse image

@param[in]: sparse_file_handle          File handler to be freed

@dependencies
sparse_file_handle must be malloced previously

@sideeffects
None

@return
None.

*/
void sparse_close (struct sparse_handle_t *);

/* ============================================================================
**  Function : read_sparse_header
** ============================================================================
*/
/**
@brief
Extracts main header from sparse image

@detail
Extracts main header from sparse image

@param[in]: fd              File pointer associated with sparse image

@dependencies
fd must be at top of the file

@sideeffects
None

@return
Struct containing all header information

*/
struct sparse_header_t read_sparse_header (FILE * fd);

/* ============================================================================
**  Function : TestIfSparse
** ============================================================================
*/
/**
    @brief
    Extracts main header from a file to check if its a sparse image

    @detail
    Extracts main header from a file to check if its a sparse image

    @param[in]: fd              File pointer associated with test image

    @dependencies
    fd must be at top of the file

    @sideeffects
    None

    @return
    True if sparse image. False otherwise.

*/
boolean TestIfSparse (FILE * fd);

/* ============================================================================
**  Function : read_chunk_header
** ============================================================================
*/
/**
@brief
Reads chunk headers until we reach a RAW chunk header

@detail
Reads chunk headers until we reach a RAW chunk header. Keeps reading if it sees DONT_CARE

@param[in]: fd              File pointer associated with sparse image
@param[in]: chunk_hdr_sz    Size of header gotten from sparse_header. Used for sanity check
@param[in]: blk_sz          Block size gotten from sparse_header. Used for sanity check

@dependencies
Assumes fd is pointing at a chunk header

@sideeffects
None

@return
Returns total number of bytes in a group

*/
long long unsigned read_chunk_header (FILE * fd, uint16_t chunk_hdr_sz, uint32_t blk_sz);

/* ============================================================================
**  Function : sparse_read_handle_init
** ============================================================================
*/
/**
    @brief
    Initializes sparse read handle

    @detail
    Reads past the sparse header and initializes the sparse_read_handle with bytes remaining and first chunk info

    @param[in]: fd                      File pointer associated with sparse image
    @param[in]: file_sector_offset      Location of the first chunk of raw data
    @param[in]: FileSize                Total size of the group of data
    @param[in]: sparse_read_handle      Read handle to be initialized

    @dependencies
    Assumes fd is pointing at sparse header

    @sideeffects
    None

    @return
    Returns the sparse header information

*/
struct sparse_header_t sparse_read_handle_init (FILE * fd, SIZE_T file_sector_offset, SIZE_T FileSize, struct sparse_read_info_t * sparse_read_handle);

/* ============================================================================
**  Function : GenerateSparseXMLTags
** ============================================================================
*/
/**
    @brief
    Generates the separate XML tags for each group of data within the sparse image

    @detail
    Generates the separate XML tags for each group of data within the sparse image

    @param[in]: fd                      File pointer associated with sparse image

    @dependencies
    None

    @sideeffects
    None

    @return
    None

*/
void GenerateSparseXMLTags(FILE * fd, char * filename, SIZE_T start_sector, SIZE_T SECTOR_SIZE_IN_BYTES, SIZE_T physical_partition);
/* ============================================================================
**  Function : AdjustPadding
** ============================================================================
*/
/**
    @brief
    calculates how many padded bytes there are in FFU image
    
    @detail
    calculates how many padded bytes there are in FFU image and further
    calculates the new byte offset after the padding.
    
    @param[in]: chunkSizeInBytes        chunk size in bytes
    @param[in]: sizeOfArea              Generally refers to offset within file
    
    @dependencies
    None
    
    @sideeffects
    None
    
    @return
    Returns new offset after taking padding into account
    
*/
uint64_t AdjustPadding(uint64_t chunkSizeInBytes, uint64_t sizeOfArea);

/* ============================================================================
**  Function : InitializeFFU
** ============================================================================
*/
/**
    @brief
    Allocates space for a new FFU struct
    
    @detail
    Allocates space for a new FFU struct and initializes some meta info for FFU
    
    @param[in]: filename                name of FFU file associated with this struct
    @param[in]: sector_size_in_bytes    sector_size_in_bytes
    @param[in]: phys_partition_number   physical partition number

    @dependencies
    None
    
    @sideeffects
    None
    
    @return
    A malloced FFUImage struct
    
*/
struct FFUImage_t * InitializeFFU(char * filename, SIZE_T sector_size_in_bytes, SIZE_T phys_partition_number);

/* ============================================================================
**  Function : CloseFFU
** ============================================================================
*/
/**
    @brief
    Deletes and frees FFU struct
    
    @detail
    Deletes and frees FFU struct
    
    @param[in]: FFU        FFU struct to be freed

    @dependencies
    FFU struct must have been malloced before
    
    @sideeffects
    None
    
    @return
    None
    
*/
void CloseFFU(struct FFUImage_t * FFU);

/* ============================================================================
**  Function : ParseFFUHeaders
** ============================================================================
*/
/**
    @brief
    Reads and parses through FFU image header
    
    @detail
    Reads through the FFU image and populates FFU struct with all the information
    
    @param[in]: FFU        FFU struct associated with the FFU image to store info. 
    @param[in]: fdp        File pointer associated with the FFU image

    @dependencies
    FFU should've been malloced previously
    
    @sideeffects
    None
    
    @return
    None. FFU is altered in function
    
*/
void ParseFFUHeaders(struct FFUImage_t * FFU, FILE * fd);

/* ============================================================================
**  Function : ReadFFUGPT
** ============================================================================
*/
/**
    @brief
    Reads and parses through the GPT in FFU
    
    @detail
    Reads and parses through the GPT in FFU. Verifies everythign is in order
    
    @param[in]: FFU        FFU struct associated with the FFU image to store info. 
    @param[in]: fdp        File pointer associated with the FFU image

    @dependencies
    None
    
    @sideeffects
    None
    
    @return
    None.
    
*/
void ReadFFUGPT(struct FFUImage_t * FFU, FILE * fd);

/* ============================================================================
**  Function : DumpFFURawProgram
** ============================================================================
*/
/**
    @brief
    Generates the separate XML tags corresponding to each group of data
    
    @detail
    Generates the separate XML tags corresponding to each group of data
    
    @param[in]: FFU        FFU struct associated with the FFU image. 

    @dependencies
    None
    
    @sideeffects
    None
    
    @return
    None.
    
*/
void DumpFFURawProgram(struct FFUImage_t * FFU);

/* ============================================================================
**  Function : CreateFFUXMLTag
** ============================================================================
*/
/**
    @brief
    Creates a single XML tag associated with a group of data within FFU
    
    @detail
    Creates a single XML tag associated with a group of data within FFU and stores
    it into the XML tree
    
    @param[in]: FFU                 FFU struct associated with the FFU image. 
    @param[in]: FileOffset          Sector offset within the file that data starts at
    @param[in]: StartSector         Start sector that data will start in
    @param[in]: NumSectors          NumSectors that data spans across

    @dependencies
    None
    
    @sideeffects
    None
    
    @return
    None.
    
*/
void CreateFFUXMLTag(struct FFUImage_t * FFU, uint64_t FileOffset, int64_t StartSector, uint64_t NumSectors);

/* ============================================================================
**  Function : TestIfWindowsFFU
** ============================================================================
*/
/**
    @brief
    Reads header of file to test if file is a windows FFU image
    
    @detail
    Checks to see if the "SignedImage" string is in the headers that
    identifies windows FFU images.
    
    @param[in]: fd              File handler associated with file to test

    @dependencies
    None
    
    @sideeffects
    None
    
    @return
    True/False depending on if file is a windows FFU image
    
*/
boolean TestIfWindowsFFU(FILE * fd);

firehose_tag_data_t firehose_tag_data[] =
{
    {"program", handleProgram},
    {"firmwarewrite", handleFirmwareWrite},
    {"configure", handleConfigure},
    {"power", handlePower},
    {"setbootablestoragedrive", handleSetBootableStorageDrive},
    {"nop", handleNop},
    {"erase", handleErase},
    {"read", handleRead},
    {"log", handleLog},
    {"response", handleResponse},
    {"options", handleOptions},
    {"file_mappings", handleFileMappings},
    {"search_paths", handleSearchPaths},
    {"rawprogram", handleRawProgram},
    {"patch", handlePatchProgram},
    {"benchmark", handleBenchmark},

};

int HasAPathCharacter (char *sz, SIZE_T Length)
{
    SIZE_T i;


    for (i = 0; i < Length; i++)
    {
        if (sz[i] == ':')
        {
            // could be c:\blah\blah
            return 1;
        }
    }

    if (sz[0] == '\\' && sz[1] == '\\')
    {
        return 1;
    }

    /*
      for(i=0;i<Length;i++)
      {
        if(sz[i]=='/')  { return 1; }
        if(sz[i]=='\\') { return 1; }
      }
    */

    return 0;
}

int IsARelativePath (char *sz, SIZE_T Length)
{
    SIZE_T i;

    for (i = 0; i < Length; i++)
    {
        if (sz[i] == ':')
        {
            // could be c:\blah\blah
            return 1;
        }
    }

    if (sz[0] == '\\' && sz[1] == '\\')
    {
        return 1;
    }

    return 0;
}



SIZE_T IsEmptySpace (char c)
{
    if ( c == 0xA )
    {
        return 1;
    }

    if ( c == 0xD )
    {
        return 1;
    }

    if ( c == 0x9 )
    {
        return 1;
    }

    if ( c == 0x20)
    {
        return 1;
    }

    return 0;
}

#define MAX_ATTR_NAME_SIZE 64
#define MAX_ATTR_RAW_SIZE 64

struct Attributes_Struct
{
    char    Name[MAX_ATTR_NAME_SIZE]; // i.e. "MaxPayloadSizeToTargetInBytes"
    char    Raw[1024];    // On PC this handles log values i.e. "8192"
    //char    *Raw;   // On PC this handles log values i.e. "8192"
    void   *pValue;
    SIZE_T  Min;
    SIZE_T  Max;
    SIZE_T  MultipleOf;
    SIZE_T  SizeOfpStr;
    char    *pStr;
    char    Type;           // i.e. "i" for integer, "s" for string, "x" for special, "t" for SIZE_T
    char    Assigned;       // i.e. Flag indicating if it was just assigned
};

struct ContentsXML_Struct
{
    SIZE_T  Address;
    char    Filename[MAX_ATTR_NAME_SIZE];
    char  Path[MAX_PATH_SIZE];
    char    Flavor[MAX_ATTR_NAME_SIZE];
    char    StorageType;  // 0=unknown,'e'=emmc,'u'=ufs,'n'=nand
    char  FileType;   // 0=unknown,'r'=<partition_file (rawprogram),'p'=<partition_patch_file (patch)
};

#define MAX_CONTENTS_XML_ENTRIES 200
struct ContentsXML_Struct ContensXML[MAX_CONTENTS_XML_ENTRIES];
struct ContentsXML_Struct ContensXMLPath[MAX_CONTENTS_XML_ENTRIES];
SIZE_T NumContensXML = 0, NumContentsXMLPath = 0;

struct ufs_extras_type
{
    char bNumberLU, bBootEnable, bDescrAccessEn, bInitPowerMode, bHighPriorityLUN, bSecureRemovalType, bInitActiveICCLevel, bConfigDescrLock;
    short wPeriodicRTCUpdate;
};

struct storage_extras_type
{
    struct ufs_extras_type ufs_extras;
};

struct storage_extras_type storage_extras;

struct UFS_LUN_Var_Struct_type
{
    char LUNum, bLUEnable, bBootLunID, bLUWriteProtect, bMemoryType, bDataReliability, bLogicalBlockSize, bProvisioningType;
    short wPeriodicRTCUpdate, wContextCapabilities;
    SIZE_T size_in_KB;
};

struct UFS_LUN_Var_Struct_type UFS_LUN_Var_Struct;

typedef struct
{
    SIZE_T MaxPayloadSizeToTargetInBytes;     // numeric attributes
    SIZE_T MaxPayloadSizeToTargetInBytesSupported;  // DDR version would typically have this set to 1MB
    SIZE_T MaxPayloadSizeFromTargetInBytes;
    SIZE_T MaxDigestTableSizeInBytes;
    SIZE_T AckRawDataEveryNumPackets;
    SIZE_T delayinseconds;
    SIZE_T address64;
    SIZE_T value64;
    SIZE_T storagedrive;
    SIZE_T SECTOR_SIZE_IN_BYTES;
    SIZE_T byte_offset;
    SIZE_T physical_partition_number;
    SIZE_T size_in_bytes;
    SIZE_T file_sector_offset;

    SIZE_T ZlpAwareHost;        // bool attributes but made SIZE_T.
    SIZE_T SkipWrite;     // otherwise the generic function that assigns values
    SIZE_T BufferWrites;
    //SIZE_T AckRawData;
    //SIZE_T display;
    //SIZE_T read_back_verify;
    SIZE_T AlwaysValidate;      // will possibly overwrite the next address in memory
    SIZE_T Verbose;
    SIZE_T commit;
    SIZE_T trials;
    //SIZE_T display;
    SIZE_T TestWritePerformance;
    SIZE_T TestReadPerformance;
    SIZE_T TestDigestPerformance;
    //SIZE_T read_back_verify;
    SIZE_T SkipStorageInit;
    SIZE_T SkipSettingMinClocks;
    SIZE_T SkipSettingMaxClocks;
    SIZE_T actual_size_in_bytes;

    //SIZE_T start_sector;    // special attributes
    char start_sector[MAX_STRING_SIZE];    // special attributes
    SIZE_T num_partition_sectors;
    char filename[MAX_STRING_SIZE];
    char value[1024 * 1024]; //On PC, this line handles log messages char value[MAX_STRING_SIZE];

    char MemoryName[MAX_STRING_SIZE];
    char TargetName[MAX_STRING_SIZE];
    char Commit[MAX_STRING_SIZE];       // We have some commit='true', which eventually goes to set fh.attrs.commit=1

    // these are stored into first in preParseAttributes(), then
    // parseAttributes() fills in start_sector and num_partition_sectors
    //char start_sector_value[64];
    //char num_partition_sectors_value[64];

    //hotplug_dev_type storage_type;  // attribute MemoryName={"eMMC" or "ufs"}
} firehose_attrs_t;


typedef struct
{
    firehose_attrs_t attrs;
} firehose_protocol_t;

firehose_protocol_t fh;


struct Attributes_Struct AllAttributes[] =
{
    {"Verbose",                             "", (SIZE_T *)&fh.attrs.Verbose,                       0, 0, 1, 0, NULL, 'i', 0 },
    {"MaxPayloadSizeToTargetInBytes",       "", (SIZE_T *)&fh.attrs.MaxPayloadSizeToTargetInBytes, 0, 0, 512, 0, NULL, 'i', 0 },
    {"MaxPayloadSizeToTargetInBytesSupported", "", (SIZE_T *)&fh.attrs.MaxPayloadSizeToTargetInBytesSupported, 0, 0, 512, 0,  NULL, 'i', 0 },
    {"MaxPayloadSizeFromTargetInBytes",      "", (SIZE_T *)&fh.attrs.MaxPayloadSizeFromTargetInBytes, 0, 0, 512, 0, NULL, 'i', 0 },


    {"MaxDigestTableSizeInBytes", "", (SIZE_T *)&fh.attrs.MaxDigestTableSizeInBytes,     0, 0, 512, 0, NULL, 'i', 0 },
    {"AckRawDataEveryNumPackets", "", (SIZE_T *)&fh.attrs.AckRawDataEveryNumPackets,     0, 0, 1, 0, NULL, 'i', 0 },
    {"delayinseconds",            "", (SIZE_T *)&fh.attrs.delayinseconds,                0, 0, 1, 0, NULL, 'i', 0 },
    {"address64",                 "", (SIZE_T *)&fh.attrs.address64,                     0, 0, 1, 0, NULL, 'i', 0 },
    {"value64",                   "", (SIZE_T *)&fh.attrs.value64,                       0, 0, 1, 0, NULL, 'i', 0 },
    //{"storagedrive",              "", (SIZE_T *)&fh.attrs.storagedrive,                  0, 0, 1, 0, NULL, 'i', 0 },
    {"storagedrive",              "", (SIZE_T *)&fh.attrs.physical_partition_number,     0, 0, 1, 0, NULL, 'i', 0 },
    {"SECTOR_SIZE_IN_BYTES",      "", (SIZE_T *)&fh.attrs.SECTOR_SIZE_IN_BYTES,          0, 0, 1, 0, NULL, 'i', 0 },
    {"byte_offset",               "", (SIZE_T *)&fh.attrs.byte_offset,                   0, 0, 1, 0, NULL, 'i', 0 },
    {"physical_partition_number", "", (SIZE_T *)&fh.attrs.physical_partition_number,     0, 0, 1, 0, NULL, 'i', 0 },
    {"size_in_bytes",             "", (SIZE_T *)&fh.attrs.size_in_bytes,                 0, 0, 1, 0, NULL, 'i', 0 },
    {"num_partition_sectors",     "", (SIZE_T *)&fh.attrs.num_partition_sectors,         0, 0, 1, 0, NULL, 'i', 0 },
    {"file_sector_offset",        "", (SIZE_T *)&fh.attrs.file_sector_offset,            0, 0, 1, 0, NULL, 'i', 0 }, // 13
    {"trials",                    "", (SIZE_T *)&fh.attrs.trials,                        0, 0, 1, 0, NULL, 'i', 0 }, //

    {"ZlpAwareHost",              "", (SIZE_T *)&fh.attrs.ZlpAwareHost,                  0, 0, 1, 0, NULL, 'i', 0 },
    {"SkipWrite",                 "", (SIZE_T *)&fh.attrs.SkipWrite,                     0, 0, 1, 0, NULL, 'i', 0 },
    {"BufferWrites",              "", (SIZE_T *)&fh.attrs.BufferWrites,                  0, 0, 1, 0, NULL, 'i', 0 },
    //{"AckRawData",                "", (SIZE_T *)&fh.attrs.AckRawData,                    0,0,1,0, NULL, 'i', 0 },
    {"AlwaysValidate",            "", (SIZE_T *)&fh.attrs.AlwaysValidate,                0, 0, 1, 0, NULL, 'i', 0 },
    {"commit",                    "", NULL,                                              0, 0, 1, sizeof (fh.attrs.Commit), (char *) fh.attrs.Commit, 'x', 0 }, // string convert to fh.attrs.commit
    //{"display",                   "", (SIZE_T *)&fh.attrs.display,                       0,0,1,0, NULL, 'i', 0 },
    //{"read_back_verify",          "", (SIZE_T *)&fh.attrs.read_back_verify,              0,0,1,0, NULL, 'i', 0 },
    {"TestWritePerformance",      "", (SIZE_T *)&fh.attrs.TestWritePerformance,          0, 0, 1, 0, NULL, 'i', 0 },
    {"TestReadPerformance",       "", (SIZE_T *)&fh.attrs.TestReadPerformance,           0, 0, 1, 0, NULL, 'i', 0 },
    {"TestDigestPerformance",     "", (SIZE_T *)&fh.attrs.TestDigestPerformance,         0, 0, 1, 0, NULL, 'i', 0 }, // 20
    {"SkipStorageInit",           "", (SIZE_T *)&fh.attrs.SkipStorageInit,               0, 0, 1, 0, NULL, 'i', 0 }, // <configure SkipStorageInit="1"
    {"SkipSettingMinClocks",      "", (SIZE_T *)&fh.attrs.SkipSettingMinClocks,          0, 0, 1, 0, NULL, 'i', 0 }, // <configure SkipSettingMinClocks="1"
    {"SkipSettingMaxClocks",      "", (SIZE_T *)&fh.attrs.SkipSettingMaxClocks,          0, 0, 1, 0, NULL, 'i', 0 }, // <configure SkipSettingMaxClocks="1"
    {"actual_size_in_bytes",      "", (SIZE_T *)&fh.attrs.actual_size_in_bytes,          0, 0, 1, 0, NULL, 'i', 0 }, // <configure actual_size_in_bytes="1234"

    /*
            // eMMC GPP creation
            {"DRIVE4_SIZE_IN_KB",                   "", (SIZE_T *)&storage_extras.emmc_extras.GPP_size[0],    0,0,512,0, NULL, 'w', 0 },    // 24
            {"DRIVE5_SIZE_IN_KB",                   "", (SIZE_T *)&storage_extras.emmc_extras.GPP_size[1],    0,0,512,0, NULL, 'w', 0 },
            {"DRIVE6_SIZE_IN_KB",                   "", (SIZE_T *)&storage_extras.emmc_extras.GPP_size[2],    0,0,512,0, NULL, 'w', 0 },
            {"DRIVE7_SIZE_IN_KB",                   "", (SIZE_T *)&storage_extras.emmc_extras.GPP_size[3],    0,0,512,0, NULL, 'w', 0 },
            {"ENH_SIZE",                            "", (SIZE_T *)&storage_extras.emmc_extras.ENH_size,       0,0,1,0,   NULL, 'w', 0 },
            {"ENH_START_ADDR",                      "", (SIZE_T *)&storage_extras.emmc_extras.ENH_start_addr, 0,0,1,0,   NULL, 'w', 0 },
            {"GPP_ENH_FLAG",                        "", (SIZE_T *)&storage_extras.emmc_extras.GPP_enh_flag,   0,0,1,0,   NULL, 'b', 0 },    // 30
    */
    // UFS LUN creation
    {"bNumberLU",                 "", (byte *)&storage_extras.ufs_extras.bNumberLU,           0, 0, 1, 0,  NULL, 'b', 0 },
    {"bBootEnable",               "", (byte *)&storage_extras.ufs_extras.bBootEnable,         0, 0, 1, 0,  NULL, 'b', 0 },
    {"bDescrAccessEn",            "", (byte *)&storage_extras.ufs_extras.bDescrAccessEn,      0, 0, 1, 0,  NULL, 'b', 0 },
    {"bInitPowerMode",            "", (byte *)&storage_extras.ufs_extras.bInitPowerMode,      0, 0, 1, 0,  NULL, 'b', 0 },
    {"bHighPriorityLUN",          "", (byte *)&storage_extras.ufs_extras.bHighPriorityLUN,    0, 0, 1, 0,  NULL, 'b', 0 },
    {"bSecureRemovalType",        "", (byte *)&storage_extras.ufs_extras.bSecureRemovalType,  0, 0, 1, 0,  NULL, 'b', 0 },
    {"bInitActiveICCLevel",       "", (byte *)&storage_extras.ufs_extras.bInitActiveICCLevel, 0, 0, 1, 0,  NULL, 'b', 0 },
    {"wPeriodicRTCUpdate",        "", (short *)&storage_extras.ufs_extras.wPeriodicRTCUpdate, 0, 0, 1, 0,  NULL, 'n', 0 },
    {"bConfigDescrLock",          "", (byte *)&storage_extras.ufs_extras.bConfigDescrLock,    0, 0, 1, 0,  NULL, 'b', 0 }, // 39

    // These vars are handled in ParseComplicatedAttributes when (CurrentHandlerFunction==handleStorageExtras)
    {"LUNum",                     "", (byte *)&UFS_LUN_Var_Struct.LUNum,                 0, 7, 1, 0, NULL, 'i', 0 },
    {"bLUEnable",                 "", (byte *)&UFS_LUN_Var_Struct.bLUEnable,             0, 0, 1, 0, NULL, 'b', 0 },
    {"bBootLunID",                "", (byte *)&UFS_LUN_Var_Struct.bBootLunID,            0, 0, 1, 0, NULL, 'b', 0 },
    {"bLUWriteProtect",           "", (byte *)&UFS_LUN_Var_Struct.bLUWriteProtect,       0, 0, 1, 0, NULL, 'b', 0 },
    {"bMemoryType",               "", (byte *)&UFS_LUN_Var_Struct.bMemoryType,           0, 0, 1, 0, NULL, 'b', 0 },
    {"size_in_KB",                "", (SIZE_T *)&UFS_LUN_Var_Struct.size_in_KB,          0, 0, 1, 0, NULL, 'i', 0 },
    {"bDataReliability",          "", (byte *)&UFS_LUN_Var_Struct.bDataReliability,      0, 0, 1, 0, NULL, 'b', 0 },
    {"bLogicalBlockSize",         "", (byte *)&UFS_LUN_Var_Struct.bLogicalBlockSize,     0, 0, 1, 0, NULL, 'b', 0 },
    {"bProvisioningType",         "", (byte *)&UFS_LUN_Var_Struct.bProvisioningType,     0, 0, 1, 0, NULL, 'b', 0 },
    {"wContextCapabilities",      "", (short *)&UFS_LUN_Var_Struct.wContextCapabilities, 0, 0, 1, 0, NULL, 'n', 0 }, // 49

    {"MemoryName",                "", NULL,                         0, 0, 1,    sizeof (fh.attrs.MemoryName), (char *) fh.attrs.MemoryName, 's', 0 },
    {"TargetName",                "", NULL,                         0, 0, 1,    sizeof (fh.attrs.TargetName), (char *) fh.attrs.TargetName, 's', 0 },
    {"filename",                  "", NULL,                         0, 0, 1,    sizeof (fh.attrs.filename),  (char *) fh.attrs.filename,   's', 0 },
    {"value",                     "", NULL,                         0, 0, 1,    sizeof (fh.attrs.value),  (char *) fh.attrs.value,      's', 0 }, // 53

    // x means it needs special processing later, as in start_sector="NUM_DISK_SECTORS-33."
    //{"start_sector",                        "", (SIZE_T *)&fh.attrs.start_sector,             0,0,1,0, NULL, 'x', 0 },   // 54
    {"start_sector",                        "", NULL,                         0, 0, 1,    sizeof (fh.attrs.start_sector), (char *) fh.attrs.start_sector, 's', 0 }, // 54
};




void print_usage (FILE *stream)
{
    ShowCommandLineOptions();

    fprintf (stream, "\nDebugging\n"
             "\nBy default --loglevel=1. This is a decent amount of logging for debugging."
             "\n           --loglevel=2. This shows more information such as RAW packets being sent."
             "\n           --loglevel=3. This is very verbose, showing every byte in a HEX editor view that is sent/received."
             "\n           --loglevel=0. Turns off nearly all logging."
             "\n           --showdigestperpacket - This shows the SHA256 digest for every packet sent"
             "\n           --createdigests --createcommandtrace - Adding createcommandtrace creates command_trace.txt - This shows every packet and it's hash"
            );
    fprintf (stream, "\nUsage: \n"
             "fh_loader.exe --port=\\\\.\\COM19 --sendimage=AnyFile.bin --search_path=c:\\builds\\Perforce_main\\core\\storage\\tools\\ --noreset --noprompt\n"
             "fh_loader.exe --port=\\\\.\\COM19 --sendimage=AnyFile.bin --search_path=c:\\builds\\Perforce_main\\core\\storage\\tools\\ --noreset --noprompt --interactive=1 \n\n"
             "fh_loader.exe --port=\\\\.\\COM19 --sendimage=AnyFile.bin --search_path=c:\\builds\\Perforce_main\\core\\storage\\tools\\ --noreset --noprompt --interactive=2 \n\n"
             "fh_loader.exe --port=\\\\.\\COM19 --sendxml=rawprogram0.xml --search_path=c:\\builds\\Perforce_main\\core\\storage\\tools\\ \n"
             "fh_loader.exe --port=\\\\.\\COM19 --sendxml=rawprogram0.xml --search_path=c:\\builds\\Perforce_main\\core\\storage\\tools\\ --loglevel=2\n"
             "fh_loader.exe --port=\\\\.\\COM19 --sendxml=rawprogram0.xml --search_path=c:\\builds\\Perforce_main\\core\\storage\\tools\\ --createcommandtrace\n"
            );

    return;
}

#ifdef _MSC_VER // i.e. if compiling under Windows
HANDLE port_fd = NULL;
#else
int port_fd = 0;
#endif

int ret = 0;

#define MAX_READ_BUFFER_SIZE ONE_MEGABYTE
SIZE_T MaxBytesToReadFromUSB = MAX_READ_BUFFER_SIZE;  // this is changed to fh.attrs.MaxPayloadSizeFromTargetInBytes after handleConfigure()

char ReadBuffer[MAX_READ_BUFFER_SIZE];
long CharsInBuffer = 0, PacketLoc = 0, PacketStart = 0;

char WarningsBuffer[ONE_MEGABYTE];
SIZE_T NumWarnings = 0;

#ifdef _MSC_VER // i.e. if compiling under Windows
COMMTIMEOUTS timeouts;
#else
struct termios tio;
struct termios settings;
int retval;
#endif

#define PRETTYPRINT(buffer, length, MaxLength) printBuffer(buffer, length, MaxLength, __FUNCTION__, __LINE__)

#define memscpy(dest,destsize,src,srcsize) memcpy(dest,src,srcsize)

#ifdef _MSC_VER
#define dbg(log_level, fmt, ...) MyLog(log_level, __FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#else
#define dbg(log_level, fmt ...) MyLog(log_level, __FUNCTION__, __LINE__, fmt)
#endif

unsigned int OpenPort (char *pData);
void ClosePort (void);
void SetPortTimeouts (void);
unsigned int WritePort (unsigned char *pData, unsigned int length, unsigned int MaxLength, unsigned char RawData);
unsigned int ReadPort (unsigned char *pData, unsigned int length, unsigned int MaxLength);
//void printBuffer(unsigned char *buffer, unsigned int length);
void printBuffer (unsigned char *buffer, unsigned int length, unsigned int MaxLength, const char *func_name, int line_number);

void MyLog (int log_level, const char *func_name, int line_number, const char *format, ...);


unsigned char VerboseLevel  = LOG_INFO, PrettyPrintRawPacketsToo = 0, ReadBogusDataBeforeSendingConfigure = 0;
unsigned char Simulate = 0, SimulateBack = 0, Interactive = 0, skipharddriveread = 0, CreateDigests = 0, Verbose = 0, wipefirst = 0, erasefirst = 0, FlattenBuild = 0, SimulateForFileSize = 0, createconfigxml = 0;
unsigned char ForceOverwrite = 0;
char WipeFirstFileName[] = "Zeros16KB.bin";
char StressTestFileName[] = "Zeros1GB.bin";

SIZE_T LastFindFileFileSize = 0;

void InitAttributes (void)
{
    fh.attrs.MaxPayloadSizeToTargetInBytes      = ONE_MEGABYTE; // Target will force this to real value after <configure> tag
    fh.attrs.MaxPayloadSizeToTargetInBytesSupported = ONE_MEGABYTE; //ONE_MEGABYTE;
    fh.attrs.MaxPayloadSizeFromTargetInBytes    = 8192; //ONE_MEGABYTE;

    MaxBytesToReadFromUSB             = fh.attrs.MaxPayloadSizeFromTargetInBytes;

    fh.attrs.Verbose                    = 0;
    fh.attrs.MaxDigestTableSizeInBytes  = 8192;
    fh.attrs.AckRawDataEveryNumPackets  = 0;
    fh.attrs.delayinseconds             = 1;
    fh.attrs.address64                  = 0;
    fh.attrs.value64                    = 0;
    fh.attrs.storagedrive               = 0;
    fh.attrs.physical_partition_number  = 0;
    fh.attrs.SECTOR_SIZE_IN_BYTES       = 512;
    fh.attrs.byte_offset                = 0;
    fh.attrs.physical_partition_number  = 0;
    fh.attrs.size_in_bytes              = 0;
    fh.attrs.num_partition_sectors      = 0;
    fh.attrs.file_sector_offset         = 0;

    fh.attrs.ZlpAwareHost               = ZLPAWAREHOST;
    fh.attrs.SkipWrite                  = 0;
    fh.attrs.BufferWrites               = 0;
    //fh.attrs.AckRawData               = 0;
    fh.attrs.AlwaysValidate             = 0;

    //fh.attrs.display                    = 0;
    //fh.attrs.read_back_verify           = 0;
    fh.attrs.TestWritePerformance       = 0;
    fh.attrs.TestReadPerformance        = 0;
    fh.attrs.TestDigestPerformance      = 0;
    fh.attrs.SkipStorageInit            = 0;
    fh.attrs.SkipSettingMinClocks       = 0;
    fh.attrs.SkipSettingMaxClocks       = 0;
    fh.attrs.actual_size_in_bytes       = 0;

    //fh.attrs.start_sector               = 0;
    //strncpy(fh.attrs.start_sector,"0",sizeof("0"));
    if ( CopyString (fh.attrs.start_sector, "0", 0, 0, sizeof ("0"), sizeof (fh.attrs.start_sector), sizeof ("0") ) == 0)
    {
        dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.start_sector", "0", strlen ("0") );
        ExitAndShowLog (1);
    }

    //strncpy(fh.attrs.MemoryName,"eMMC",sizeof("eMMC"));
    if ( CopyString (fh.attrs.MemoryName, "eMMC", 0, 0, sizeof ("eMMC"), sizeof (fh.attrs.MemoryName), sizeof ("eMMC") ) == 0)
    {
        dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.MemoryName", "eMMC", strlen ("eMMC") );
        ExitAndShowLog (1);
    }

}

void InitLogging (void)
{

    fp = ReturnFileHandle (PortTraceName, MAX_PATH_SIZE, "w");    // will exit if not successful
    //fclose(fp);
    //setvbuf(stdout, NULL, _IOFBF, PORT_TRACE_LOG_BUFFER_SIZE);

    //setvbuf(fp, NULL, _IOFBF, PORT_TRACE_LOG_BUFFER_SIZE);  // aaron put this back on later
}

#define TEMP_LOG_SIZE 2048

void MyLog (int log_level, const char *func_name, int line_number, const char *format, ...)
{
    va_list args;
    char log[TEMP_LOG_SIZE], fileloglevel[TEMP_LOG_SIZE], screenloglevel[TEMP_LOG_SIZE], timelog[TEMP_LOG_SIZE];
    time_t current_time;
    struct tm *local_time;
    char PrintToScreen = 0; // assume false
    SIZE_T Length;


    if (VerboseLevel >= log_level )
        PrintToScreen = 1;

    current_time = time (NULL);
    local_time = localtime (&current_time);
    sprintf (timelog, "\n%02d:%02d:%02d:", local_time->tm_hour, local_time->tm_min, local_time->tm_sec);

    Length = strlen (format);

    if (Length < TEMP_LOG_SIZE)
    {
        va_start (args, format);
        vsnprintf (log, sizeof (log), format, args);
        va_end (args);
    }
    else
    {
        printf ("\n\nERROR: In MyLog the format string of length %"SIZE_T_FORMAT" bytes is bigger than char log[%i]", Length, TEMP_LOG_SIZE);
        ExitAndShowLog (1);
    }

    switch (log_level)
    {
    case LOG_ERROR:
        ThereWereErrors = 1;
        sprintf (fileloglevel,
                 "\n\n\n"
                 "\t _____                    \n"
                 "\t|  ___|                   \n"
                 "\t| |__ _ __ _ __ ___  _ __ \n"
                 "\t|  __| '__| '__/ _ \\| '__|\n"
                 "\t| |__| |  | | | (_) | |   \n"
                 "\t\\____/_|  |_|  \\___/|_|  \n\n"
                 "%s {ERROR: %s:%d ", timelog, func_name, line_number);

        sprintf (screenloglevel,
                 "\n\n\n"
                 "\t _____                    \n"
                 "\t|  ___|                   \n"
                 "\t| |__ _ __ _ __ ___  _ __ \n"
                 "\t|  __| '__| '__/ _ \\| '__|\n"
                 "\t| |__| |  | | | (_) | |   \n"
                 "\t\\____/_|  |_|  \\___/|_|  \n\n"
                 "%s {ERROR: ", timelog);

        break;

    case LOG_INFO:
        sprintf (fileloglevel, "%s INFO: ", timelog);
        sprintf (screenloglevel, "%s INFO: ", timelog);
        break;

    case LOG_ALWAYS:
        sprintf (fileloglevel, "%s INFO: ", timelog);
        sprintf (screenloglevel, "%s INFO: ", timelog);
        break;

    case LOG_ONLY:
        sprintf (fileloglevel, "%s DEBUG: %s:%d ", timelog, func_name, line_number);
        sprintf (screenloglevel, "%s DEBUG: ", timelog);
        break;

    case LOG_DEBUG:
        //sprintf(fileloglevel,"%s DEBUG: %s:%d ", timelog, func_name, line_number);
        sprintf (fileloglevel, "%s DEBUG: ", timelog, func_name, line_number);
        sprintf (screenloglevel, "%s DEBUG: ", timelog);
        break;

    case LOG_WARN:

        NumWarnings++;

        if ( CopyString (WarningsBuffer, "\n", strlen (WarningsBuffer), 0, strlen ("\n"), sizeof (WarningsBuffer), sizeof ("\n") )  == 0)
        {
            printf ("\n\nERROR: Failed to copy string of length %i bytes into WarningsBuffer", strlen ("\n") );
            ExitAndShowLog (1);
        }

        if ( CopyString (WarningsBuffer, log, strlen (WarningsBuffer), 0, strlen (log), sizeof (WarningsBuffer), sizeof (log) )  == 0)
        {
            printf ("\n\nERROR: Failed to copy string of length %i bytes into WarningsBuffer", strlen (log) );
            ExitAndShowLog (1);
        }

        sprintf (fileloglevel,
                 "\n\n\n"
                 "\t                         (_)            \n"
                 "\t__      ____ _ _ __ _ __  _ _ __   __ _ \n"
                 "\t\\ \\ /\\ / / _` | '__| '_ \\| | '_ \\ / _` |\n"
                 "\t \\ V  V / (_| | |  | | | | | | | | (_| |\n"
                 "\t  \\_/\\_/ \\__,_|_|  |_| |_|_|_| |_|\\__, |\n"
                 "\t                                   __/ |\n"
                 "\t                                  |___/ \n\n"
                 "%s WARNING: %s:%d ", timelog, func_name, line_number);

        sprintf (screenloglevel,
                 "\n\n\n"
                 "\t                         (_)            \n"
                 "\t__      ____ _ _ __ _ __  _ _ __   __ _ \n"
                 "\t\\ \\ /\\ / / _` | '__| '_ \\| | '_ \\ / _` |\n"
                 "\t \\ V  V / (_| | |  | | | | | | | | (_| |\n"
                 "\t  \\_/\\_/ \\__,_|_|  |_| |_|_|_| |_|\\__, |\n"
                 "\t                                   __/ |\n"
                 "\t                                  |___/ \n\n"
                 "%s WARNING: ", timelog);

        break;


    default:
        break;
    }


    if (fp != NULL)
    {
        fprintf (fp, "%s", fileloglevel);
        fprintf (fp, "%s", log);
    }


    if (PrintToScreen)
    {

        if (log_level == LOG_ERROR)
        {
            printf ("%s", screenloglevel);
            printf ("%s}", log);
        }
        else
        {
            printf ("%s", screenloglevel);
            printf ("%s", log);
        }
    }

    /*
      if (CopyString(PortTraceLogBuffer, fileloglevel, PortTraceLogBufferPos, 0, strlen(fileloglevel), PORT_TRACE_LOG_BUFFER_SIZE, 2048)  == 0)
      {
        printf("\n\nERROR: Failed to copy string of length %i bytes into PortTraceLogBuffer at position %"SIZE_T_FORMAT,strlen(fileloglevel),PortTraceLogBufferPos);
        ExitAndShowLog(1);
      }
      PortTraceLogBufferPos += strlen(fileloglevel);

      if (CopyString(PortTraceLogBuffer, log, PortTraceLogBufferPos, 0, strlen(log), PORT_TRACE_LOG_BUFFER_SIZE, 2048)  == 0)
      {
        printf("\n\nERROR: Failed to copy string of length %i bytes into PortTraceLogBuffer at position %"SIZE_T_FORMAT,strlen(log),PortTraceLogBufferPos);
        ExitAndShowLog(1);
      }
      PortTraceLogBufferPos += strlen(log);


      if (PortTraceLogBufferPos > PORT_TRACE_LOG_BUFFER_ROLLOVER_AT)
      {
        // We are 64KB away from the end of the log buffer. Obviously no problems yet.
        // So I want to preserve the first 2MB of the buffer, then blow away the next 15MB,
        RolloverCount++;
        Length = PortTraceLogBufferPos; // backup this value
        PortTraceLogBufferPos = PORT_TRACE_LOG_AMOUNT_TO_PRESERVE;

        sprintf (fileloglevel,"\n\n\n\n\nPortTraceLogBufferPos reached position %"SIZE_T_FORMAT", this is RolloverCount=%"SIZE_T_FORMAT,Length,RolloverCount);
        if( CopyString(PortTraceLogBuffer, fileloglevel, PortTraceLogBufferPos, 0, strlen(fileloglevel), PORT_TRACE_LOG_BUFFER_SIZE, 2048)  == 0)
        {
          printf("\n\nERROR: Failed to copy string of length %i bytes into PortTraceLogBuffer at position %"SIZE_T_FORMAT,strlen(fileloglevel),PortTraceLogBufferPos);
          ExitAndShowLog(1);
        }
        PortTraceLogBufferPos += strlen(fileloglevel);

        sprintf (fileloglevel,"\n\nBelow is the last %i bytes of the port_trace.txt *before* resetting PortTraceLogBufferPos to %i\n\n\n\n",
          (PORT_TRACE_LOG_AMOUNT_TO_PRESERVE+Length-PORT_TRACE_LOG_BUFFER_ROLLOVER_AT),PORT_TRACE_LOG_AMOUNT_TO_PRESERVE);

        if (CopyString(PortTraceLogBuffer, fileloglevel, PortTraceLogBufferPos, 0, strlen(fileloglevel), PORT_TRACE_LOG_BUFFER_SIZE, 2048)  == 0)
        {
          printf ("\n\nERROR: Failed to copy string of length %i bytes into PortTraceLogBuffer at position %"SIZE_T_FORMAT,strlen(fileloglevel),PortTraceLogBufferPos);
          ExitAndShowLog(1);
        }
        PortTraceLogBufferPos += strlen(fileloglevel);


        // Now copy all data from Length-1MB back to the start of the buffer
        memscpy (&PortTraceLogBuffer[PortTraceLogBufferPos], PORT_TRACE_LOG_BUFFER_SIZE, &PortTraceLogBuffer[PORT_TRACE_LOG_BUFFER_ROLLOVER_AT-PORT_TRACE_LOG_AMOUNT_TO_PRESERVE], PORT_TRACE_LOG_AMOUNT_TO_PRESERVE+Length-PORT_TRACE_LOG_BUFFER_ROLLOVER_AT);  // ONE_MEGABYTE
        PortTraceLogBufferPos+=(PORT_TRACE_LOG_AMOUNT_TO_PRESERVE+Length-PORT_TRACE_LOG_BUFFER_ROLLOVER_AT);

        sprintf (fileloglevel,"\n\nBefore was RolloverCount=%"SIZE_T_FORMAT", New stuff is after this\n\n\n\n",RolloverCount);
        if (CopyString(PortTraceLogBuffer, fileloglevel, PortTraceLogBufferPos, 0, strlen(fileloglevel), PORT_TRACE_LOG_BUFFER_SIZE, 2048)  == 0)
        {
          printf("\n\nERROR: Failed to copy string of length %i bytes into PortTraceLogBuffer at position %"SIZE_T_FORMAT,strlen(fileloglevel),PortTraceLogBufferPos);
          ExitAndShowLog(1);
        }
        PortTraceLogBufferPos += strlen(fileloglevel);

        memset(&PortTraceLogBuffer[PortTraceLogBufferPos], 0x0, PORT_TRACE_LOG_BUFFER_SIZE-PortTraceLogBufferPos);  // this is strictly not needed
      }

    */




//  if(fp!=NULL)
//    fclose(fp);

}

int getoptarg (int i, char * argv[], char * opt, SIZE_T SizeOfOpt, char *arg, SIZE_T SizeOfArg );

int getoptarg (int i, char * argv[], char * Option, SIZE_T SizeOfOpt, char * Argument, SIZE_T SizeOfArg)
{
    char *pch = NULL;
    unsigned int Length = 0;

    memset (Option, 0, SizeOfOpt);
    memset (Argument, 0, SizeOfArg);


    if (argv[i][0] != '-' || argv[i][1] != '-')
    {
        dbg (LOG_ERROR, "Option '%s' does not begin with --\n", argv[i]);
        ExitAndShowLog (1);
    }

    Length = strlen (argv[i]);

    if (Option[Length - 1] == '=')
    {
        dbg (LOG_ERROR, "Option '%s' did not supply an argument after the '=' sign\n", argv[i]);
        ExitAndShowLog (1);
    }

    if ( CopyString (Option, &argv[i][2], 0, 0, strlen (&argv[i][2]), SizeOfOpt, strlen (&argv[i][2]) )  == 0)
    {
        dbg (LOG_ERROR, "Failed to copy string of length %"SIZE_T_FORMAT" bytes into Option", strlen (&argv[i][2]) );
        ExitAndShowLog (1);
    }

    //strncpy(Option,&argv[i][2],strlen(&argv[i][2]));
    Option[strlen (&argv[i][2])] = '\0'; // ensure NULL is here
    //printf("\n\n\n\n\n\n\nOption='%s'\n\n",Option);

    // Now Option no longer has "--" at the beginning
    // So --superverbose becomes superverbose

    pch = strstr (Option, "=");

    if (pch != '\0')
    {
        //printf("pch is NOT NULL\n\n");
        // To be here pch is pointing at "=COM5", Option is pointing at "--port=COM5"

        if ( CopyString (Argument, pch, 0, 1, strlen (&pch[1]), SizeOfArg, strlen (pch) )  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string of length %"SIZE_T_FORMAT" bytes into Argument", strlen (&pch[1]) );
            ExitAndShowLog (1);
        }

        //strncpy(Argument, &pch[1], strlen(pch));    // This stores "port=" into Option
        Option[pch - Option + 1] = '\0';              // cut off just before = sign
    }

    // To be here Option is something like "port=" or "simulate" or "superverbose" or "xml="

    //printf("Option   = '%s'\n",Option);
    //printf("Argument = '%s'\n\n",Argument);

    return 0;
}

void ExitAndShowLog (unsigned int ExitCode)
{
    //SIZE_T Length;

    ClosePort();

    //fp = fopen(PortTraceName,"a");  // can't use ReturnFileHandle here, or else infinite recurrsion
    if (fp == NULL)
    {
        printf ("\n\nERROR: Could not append to '%s'", PortTraceName);
    }
    else
    {
        if (!HasAPathCharacter (PortTraceName, strlen (PortTraceName) ) )
            printf ("\n\nWriting log to '%s%s', might take a minute\n", cwd, PortTraceName);
        else
            printf ("\n\nWriting log to '%s', might take a minute\n", PortTraceName);

        //Length = strlen(PortTraceLogBuffer);
        //fprintf(fp,"%s",PortTraceLogBuffer);
        //fflush( fp );
        fclose (fp);
        fp = NULL;
    }

    if (fdp != NULL)
    {
        fclose (fdp);
        fdp = NULL;
    }

    if (fc != NULL)
    {
        fclose (fc);
        fc  = NULL;
    }

    if (ft != NULL)
    {
        fclose (ft);
        ft  = NULL;
    }

    if (fg != NULL)
    {
        fclose (fg);
        fg  = NULL;
    }

    if (!HasAPathCharacter (PortTraceName, strlen (PortTraceName) ) )
        printf ("\n\nLog is '%s%s'\n\n", cwd, PortTraceName);
    else
        printf ("\n\nLog is '%s'\n\n", PortTraceName);

    // use --noprompt to not be prompted at the end
    //PromptUser = 1; // I make this 1 when using Visual Studio, so I can see the stdout before it quits on me
    if (PromptUser)
    {
        printf ("\n\nPress any key to exit\n\n");
        getchar();
        fseek (stdin, 0, SEEK_END); // get rid of extra \n keys
    }

    exit (ExitCode);
}

void PerformSHA256 (uint8* inputPtr, uint32 inputLen, uint8* outputPtr)
{
    //struct __sechsh_ctx_s   context;

    sechsharm_sha256_init  (&context);
    sechsharm_sha256_update (&context,
                             context.leftover,
                             & (context.leftover_size),
                             inputPtr,
                             inputLen);

    sechsharm_sha256_final (&context,
                            context.leftover,
                            & (context.leftover_size),
                            outputPtr);

}


void LoadBuffer (char *MyBuffer, SIZE_T Length)
{
    SIZE_T i;

    for (i = 0; i < Length; i++)
        MyBuffer[i] = (i & 0xFF);

}

/* Get the month the binary was built.
   @param month [output] it should be at least 2 characters long.
*/
static void get_build_month (char *month)
{
    if (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
    {
        *month = '0';
        * (month + 1) = '1';
    }
    else if (__DATE__[0] == 'F' && __DATE__[1] == 'e' && __DATE__[2] == 'b')
    {
        *month = '0';
        * (month + 1) = '2';
    }
    else if (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
    {
        *month = '0';
        * (month + 1) = '3';
    }
    else if (__DATE__[0] == 'A' && __DATE__[1] == 'p' && __DATE__[2] == 'r')
    {
        *month = '0';
        * (month + 1) = '4';
    }
    else if (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
    {
        *month = '0';
        * (month + 1) = '5';
    }
    else if (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
    {
        *month = '0';
        * (month + 1) = '6';
    }
    else if (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
    {
        *month = '0';
        * (month + 1) = '7';
    }
    else if (__DATE__[0] == 'A' && __DATE__[1] == 'u' && __DATE__[2] == 'g')
    {
        *month = '0';
        * (month + 1) = '8';
    }
    else if (__DATE__[0] == 'S' && __DATE__[1] == 'e' && __DATE__[2] == 'p')
    {
        *month = '0';
        * (month + 1) = '9';
    }
    else if (__DATE__[0] == 'O' && __DATE__[1] == 'c' && __DATE__[2] == 't')
    {
        *month = '1';
        * (month + 1) = '0';
    }
    else if (__DATE__[0] == 'N' && __DATE__[1] == 'o' && __DATE__[2] == 'v')
    {
        *month = '1';
        * (month + 1) = '1';
    }
    else if (__DATE__[0] == 'D' && __DATE__[1] == 'e' && __DATE__[2] == 'c')
    {
        *month = '1';
        * (month + 1) = '2';
    }
    else
    {
        printf ("Cant convert date string to number at compile time");
        *month = __DATE__[0];
        * (month + 1) = __DATE__[2];
    }


}

/*--------------------------------------------------------
----------------SPARSE FILE FUNCTIONS---------------------
---------------------------------------------------------*/
struct sparse_handle_t * sparse_open (FILE * fd, uint64_t current_start_sector, uint64_t sector_size_in_bytes)
{
    struct sparse_handle_t * return_sparse_handle;

    struct sparse_header_t sparse_header;
    struct chunk_header_t chunk_header;
    const size_t SPARSE_HEADER_LEN = sizeof (struct sparse_header_t);
    const size_t CHUNK_HEADER_LEN = sizeof (struct chunk_header_t);
    char * buffer = NULL;

    int start_new_chunk = TRUE;
    uint64_t chunk_start_sector = 0;
    long long int chunk_file_end_offset = 0;
    int i = 0;
    long long unsigned temp;
    size_t result;
    uint64_t file_size_sectors;

    long long unsigned chunk = 0;
    long long unsigned length = 0;

    long long int file_offset;

    uint32_t total_blocks = 0;
    int cur_group = -1;

    //Do a pass through the file to figure out how many bytes we need to read
    result = fread (&sparse_header, sizeof (char), SPARSE_HEADER_LEN, fd);

    if (result != SPARSE_HEADER_LEN)
    {
        dbg (LOG_ERROR, "Didn't properly read the sparse_header!\n");
        ExitAndShowLog (1);
    }

    //Check if headers are correct
    if (sparse_header.magic != SPARSE_HEADER_MAGIC)
    {
        dbg (LOG_ERROR, "Bad magic: %x probably not processing a sparse image\n", sparse_header.magic);
        ExitAndShowLog (1);
    }

    if (sparse_header.major_version != SPARSE_HEADER_MAJOR_VER)
    {
        dbg (LOG_ERROR, "Unknown major version number\n", sparse_header.major_version);
        ExitAndShowLog (1);
    }

    //Allocate space based on sparse header, this is worst case space allocation
    return_sparse_handle = (struct sparse_handle_t*) malloc (sizeof (struct sparse_handle_t) );

    if (return_sparse_handle == NULL)
    {
        dbg (LOG_ERROR, "Malloc for return sparse handle failed");
        ExitAndShowLog (1);
    }

    return_sparse_handle->start_sector = malloc (sparse_header.total_chunks*sizeof (unsigned long long) );
    return_sparse_handle->bytes_remaining = malloc (sparse_header.total_chunks*sizeof (unsigned long long) );
    return_sparse_handle->group_file_offset = malloc (sparse_header.total_chunks*sizeof (long int) );

    if (return_sparse_handle->start_sector == NULL || return_sparse_handle->bytes_remaining == NULL || return_sparse_handle->group_file_offset == NULL)
    {
        dbg (LOG_ERROR, "Malloc in sparse_open failed");
        ExitAndShowLog (1);
    }

    return_sparse_handle->fd = fd;
    return_sparse_handle->total_groups = 0;
    return_sparse_handle->chunk_header_idx = 0;


    if (sparse_header.file_hdr_sz > SPARSE_HEADER_LEN)
    {
        //skip the remaining bytes in a header that is longer than we expected
        fseek (fd, sparse_header.file_hdr_sz - SPARSE_HEADER_LEN, SEEK_CUR);
    }

    //Loop through the chunks
    for (i = 0; i < sparse_header.total_chunks; i++)
    {
        file_offset = ftell (fd);
        result = fread (&chunk_header, sizeof (char), CHUNK_HEADER_LEN, fd);

        if (result != CHUNK_HEADER_LEN)
        {
            dbg (LOG_ERROR, "Didn't get proper amount of bytes for chunk_header\n");
            ExitAndShowLog (1);
        }

        if (sparse_header.chunk_hdr_sz > CHUNK_HEADER_LEN)
        {
            //Skip the remaining bytes in a header that is longer than we expected
            fseek (fd, sparse_header.chunk_hdr_sz - CHUNK_HEADER_LEN, SEEK_CUR);
        }

        if (chunk_header.chunk_type == CHUNK_TYPE_RAW)
        {
            //Actually handle the splitting here
            //will have chunk_header.chunk_sz blocks of raw data
            if (chunk_header.total_sz != (sparse_header.chunk_hdr_sz + (chunk_header.chunk_sz * sparse_header.blk_sz) ) )
            {
                dbg (LOG_ERROR, "Bogus chunk size for chunk %d, type Raw\n", i);
                ExitAndShowLog (1);
            }

            temp = chunk_header.chunk_sz * sparse_header.blk_sz;

            if ( (temp % sector_size_in_bytes) == 0)
            {
                file_size_sectors = temp / sector_size_in_bytes;
            }
            else
            {
                dbg (LOG_ERROR, "File chunk size %d is not a sector-multiple", temp);
                ExitAndShowLog (1);
            }

            if (start_new_chunk)
            {
                //First RAW we encounter
                cur_group++;
                return_sparse_handle->total_groups = cur_group + 1;
                //Create a new file for writing data
                start_new_chunk = FALSE;
                return_sparse_handle->group_file_offset[cur_group] = file_offset;
                return_sparse_handle->start_sector[cur_group] = current_start_sector;
                return_sparse_handle->bytes_remaining[cur_group] = 0;
            }

            //Pull out all length bytes of data from file
            length = chunk_header.chunk_sz * sparse_header.blk_sz;

            while (length != 0)
            {
                chunk = (length > COPY_BUF_SIZE) ? COPY_BUF_SIZE : length;

                if (fseek (fd, chunk, SEEK_CUR) != 0)
                {
                    dbg (LOG_ERROR, "Error when trying to read RAW data from file\n");
                    ExitAndShowLog (1);
                }

                length -= chunk;

                return_sparse_handle->bytes_remaining[cur_group] += chunk;
            } //end while(length != 0)

            current_start_sector += file_size_sectors;
            total_blocks = total_blocks + chunk_header.chunk_sz;
        } //end if(chunk_header.chunk_type == CHUNK_TYPE_RAW)
        else if (chunk_header.chunk_type == CHUNK_TYPE_DONT_CARE)
        {
            //Useless data that does not need to be sent
            //signifies the end of one group
            if (chunk_header.total_sz != sparse_header.chunk_hdr_sz)
            {
                dbg (LOG_ERROR, "Bogus chunk size for chunk %d, type Don't Care\n", i);
                ExitAndShowLog (1);
            }

            if (start_new_chunk == FALSE) //We encountered a CHUNK_RAW before this
            {
                start_new_chunk = TRUE;
            }

            temp = chunk_header.chunk_sz * sparse_header.blk_sz;

            if ( (temp % sector_size_in_bytes) == 0)
            {
                current_start_sector += (temp / sector_size_in_bytes);
            }
            else
            {
                dbg (LOG_ERROR, "Number of bytes skipped %d is not a sector-multiple\n", temp);
                ExitAndShowLog (1);
            }

            total_blocks = total_blocks + chunk_header.chunk_sz;
        } //end if(chunk_header.chunk_type == CHUNK_TYPE_DONT_CARE)
        else
        {
            dbg (LOG_ERROR, "Unknown chunk type %x\n", chunk_header.chunk_type);
            ExitAndShowLog (1);
        }
    } //End for loop through total_blocks

    //Sanity check to make sure we didnt miss any blocks
    if (sparse_header.total_blks != total_blocks)
    {
        dbg (LOG_ERROR, "Wrote %d blocks, expected to write %d blocks\n", total_blocks, sparse_header.total_blks);
        ExitAndShowLog (1);
    }

    fseek (fd, 0, SEEK_SET);

    return return_sparse_handle;
}

//Assumes sparse open was called before this
size_t sparse_read (char * data, size_t num_bytes, FILE * fd, struct sparse_read_info_t * sparse_read_struct, struct sparse_header_t sparse_header)
{
    const size_t SPARSE_HEADER_LEN = sizeof (struct sparse_header_t);
    const size_t CHUNK_HEADER_LEN = sizeof (struct chunk_header_t);
    char * buffer = NULL;

    int start_new_chunk = TRUE;
    uint32_t chunk_start_sector = 0;
    long long int chunk_file_end_offset = 0;
    int i = 0;

    uint32_t sector_size_in_bytes = 512;
    uint32_t current_start_sector = 790528;

    long long unsigned chunk = 0;
    long long unsigned length = 0;
    long long unsigned group_size_in_bytes = 0;
    long long unsigned data_offset = 0;
    uint32_t total_blocks = 0;
    size_t bytes_read = 0;

    //How many bytes are we reading out
    length = (num_bytes < sparse_read_struct->bytes_remaining) ? num_bytes : sparse_read_struct->bytes_remaining;

    //Loop through the chunks, reading one by one
    while (length > 0)
        //for (i = 0; i < length; i++)
    {
        //If current file position is at a header, read past all headers until a raw header
        if (sparse_read_struct->chunk_remaining == 0)
        {
            sparse_read_struct->chunk_remaining = read_chunk_header (fd, sparse_header.chunk_hdr_sz, sparse_header.blk_sz);
        }

        chunk = (sparse_read_struct->chunk_remaining > length) ? length : sparse_read_struct->chunk_remaining;

        //ACTUALLY READING A BYTE OF DATA
        //OPTIMIZE TO READ AS MUCH AS POSSIBLE
        if (fread (data + data_offset, sizeof (char), chunk, fd) != chunk)
        {
            dbg (LOG_ERROR, "Error when trying to read %llu bytes of data in sparse_read\n", chunk);
            ExitAndShowLog (1);
        }

        data_offset += chunk;
        bytes_read += chunk;
        sparse_read_struct->chunk_remaining -= chunk;
        sparse_read_struct->bytes_remaining -= chunk;
        length -= chunk;
    }

    return bytes_read;
}

void sparse_close (struct sparse_handle_t * sparse_file_handle)
{
    //Deletes and frees all thing associated with the struct sparse_handle_t
    free (sparse_file_handle->start_sector);
    free (sparse_file_handle->bytes_remaining);
    sparse_file_handle->start_sector = NULL;
    sparse_file_handle->bytes_remaining = NULL;
    free (sparse_file_handle);
    sparse_file_handle = NULL;
}

struct sparse_header_t read_sparse_header (FILE * fd)
{
    struct sparse_header_t sparse_header;
    const size_t SPARSE_HEADER_LEN = sizeof (struct sparse_header_t);
    size_t result;
    //READ PAST THE FIRST HEADER, DONT NEED TO DO SANITY CHECKS. ALREADY DONE IN OPEN
    result = fread (&sparse_header, sizeof (char), SPARSE_HEADER_LEN, fd);

    if (result != SPARSE_HEADER_LEN)
    {
        dbg (LOG_ERROR, "Didn't properly read the sparse_header!\n");
        ExitAndShowLog (1);
    }

    if (sparse_header.file_hdr_sz > SPARSE_HEADER_LEN)
    {
        //skip the remaining bytes in a header that is longer than we expected
        fseek (fd, sparse_header.file_hdr_sz - SPARSE_HEADER_LEN, SEEK_CUR);
    }

    return sparse_header;

}

boolean TestIfSparse (FILE * fd)
{
    struct sparse_header_t sparse_header;
    const size_t SPARSE_HEADER_LEN = sizeof (struct sparse_header_t);
    size_t result;
    //READ PAST THE FIRST HEADER, DONT NEED TO DO SANITY CHECKS. ALREADY DONE IN OPEN
    result = fread (&sparse_header, sizeof (char), SPARSE_HEADER_LEN, fd);

    if (result != SPARSE_HEADER_LEN)
    {
        fseek (fd, 0, SEEK_SET);
        return FALSE;
    }

    //Check if headers are correct
    if (sparse_header.magic != SPARSE_HEADER_MAGIC || sparse_header.major_version != SPARSE_HEADER_MAJOR_VER)
    {
        fseek (fd, 0, SEEK_SET);
        return FALSE;
    }

    fseek (fd, 0, SEEK_SET);
    return TRUE;
}

long long unsigned read_chunk_header (FILE * fd, uint16_t chunk_hdr_sz, uint32_t blk_sz)
{
    struct chunk_header_t chunk_header;
    const size_t SPARSE_HEADER_LEN = sizeof (struct sparse_header_t);
    const size_t CHUNK_HEADER_LEN = sizeof (struct chunk_header_t);
    size_t result;

    result = fread (&chunk_header, sizeof (char), CHUNK_HEADER_LEN, fd);

    if (result != CHUNK_HEADER_LEN)
    {
        dbg (LOG_ERROR, "Didn't get proper amount of bytes for chunk_header\n");
        ExitAndShowLog (1);
    }

    if (chunk_hdr_sz > CHUNK_HEADER_LEN)
    {
        //Skip the remaining bytes in a header that is longer than we expected
        fseek (fd, chunk_hdr_sz - CHUNK_HEADER_LEN, SEEK_CUR);
    }

    //keep going to the next raw header
    while (chunk_header.chunk_type == CHUNK_TYPE_DONT_CARE)
    {
        result = fread (&chunk_header, sizeof (char), CHUNK_HEADER_LEN, fd);

        if (result != CHUNK_HEADER_LEN)
        {
            dbg (LOG_ERROR, "Didn't get proper amount of bytes for chunk_header\n");
            ExitAndShowLog (1);
        }

        if (chunk_hdr_sz > CHUNK_HEADER_LEN)
        {
            //Skip the remaining bytes in a header that is longer than we expected
            fseek (fd, chunk_hdr_sz - CHUNK_HEADER_LEN, SEEK_CUR);
        }
    }

    return 1UL * chunk_header.chunk_sz * blk_sz;
}

struct sparse_header_t sparse_read_handle_init (FILE * fd, SIZE_T file_sector_offset, SIZE_T FileSize, struct sparse_read_info_t * sparse_read_handle)
{
    struct sparse_header_t sparse_header;
    sparse_header = read_sparse_header (fd); //read past the sparse header, can pull more information out of this function if needed
    //seek to correct offset (from beginning)
    fseek (fd, file_sector_offset, SEEK_SET);
    //populate first chunk header stuff for sparse_read_handle
    sparse_read_handle->bytes_remaining = FileSize;
    sparse_read_handle->chunk_remaining = read_chunk_header (fd, sparse_header.chunk_hdr_sz, sparse_header.blk_sz);
    return sparse_header;
}

void GenerateSparseXMLTags (FILE * fd, char * filename, SIZE_T start_sector, SIZE_T SECTOR_SIZE_IN_BYTES, SIZE_T physical_partition)
{
    struct sparse_handle_t * sparse_file_handle = NULL;
    SIZE_T FileSizeNumSectors = 0;
    uint32_t i;
    dbg (LOG_INFO, "Reading through sparse file '%s' and pulling out relevant header information...", filename);
    sparse_file_handle = sparse_open (fd, start_sector, SECTOR_SIZE_IN_BYTES);

    dbg (LOG_INFO, "File %s is a sparse file, being split up into %d separate XML tags", filename, sparse_file_handle->total_groups);

    for (i = 0; i < sparse_file_handle->total_groups; i++)
    {
        FileSizeNumSectors = sparse_file_handle->bytes_remaining[i] / SECTOR_SIZE_IN_BYTES;

        if (sparse_file_handle->bytes_remaining[i] % SECTOR_SIZE_IN_BYTES)
        {
            FileSizeNumSectors++;
        }

        InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
        AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "%s ", "<program "); //the program tag
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "filename=\"%s\" ", filename);

        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SECTOR_SIZE_IN_BYTES);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", FileSizeNumSectors); //sparse_file_handle->
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", physical_partition);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "start_sector=\"%"SIZE_T_FORMAT"\" ", sparse_file_handle->start_sector[i]);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "file_sector_offset=\"%"SIZE_T_FORMAT"\" ", sparse_file_handle->group_file_offset[i]);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "sparse=\"%s\" ", "true");
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "UNSPARSE_FILE_SIZE=\"%"SIZE_T_FORMAT"\" ", sparse_file_handle->bytes_remaining[i]);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        AppendToBuffer (tx_buffer, " />\n</data>", FIREHOSE_TX_BUFFER_SIZE);

        CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, FIREHOSE_TX_BUFFER_SIZE);
        NumXMLFilesInTable++;
    }

    sparse_close (sparse_file_handle);
}
/*----------------------------------------------------------------
----------------------- End Sparse File Functions-----------------
-----------------------------------------------------------------*/
/*--------------------------------------------------------
----------------FFU IMAGE FUNCTIONS---------------------
---------------------------------------------------------*/
uint64_t AdjustPadding(uint64_t chunkSizeInBytes, uint64_t sizeOfArea)
{
    uint64_t remaining_space = sizeOfArea % chunkSizeInBytes;
    //If area does not end on a boudnary, need padding. Compute.
    if(remaining_space != 0)
    {
        return sizeOfArea + (chunkSizeInBytes - remaining_space);
    }
    else
    {
        return sizeOfArea;
    }
}

struct FFUImage_t * InitializeFFU(char * filename, SIZE_T sector_size_in_bytes, SIZE_T phys_partition_number)
{
    struct FFUImage_t * ReturnFFU;
    ReturnFFU = (struct FFUImage_t *)malloc(sizeof(struct FFUImage_t));
    memcpy(ReturnFFU->filename, filename, strlen(filename)+1);
    ReturnFFU->sector_size_in_bytes = sector_size_in_bytes;
    ReturnFFU->physical_partition_number = phys_partition_number;
    return ReturnFFU;
}

void CloseFFU(struct FFUImage_t * FFU)
{
    if (FFU->FFUStoreHeader.dwValidateDescriptorLength != 0)
        free(FFU->ValidationEntries);
    if (FFU->FFUStoreHeader.dwWriteDescriptorLength != 0)
        free(FFU->BlockDataEntries);
    free(FFU);
    
    FFU->ValidationEntries = NULL;
    FFU->BlockDataEntries = NULL;
    FFU = NULL;
}

void ParseFFUHeaders(struct FFUImage_t * FFU, FILE * fd)
{
    int hashedChunkSizeInBytes;
    int hashedChunkSizeInKB;
    uint64_t currentFileOffset = 0;
    size_t result;
    struct FFUSecurityHeader_t * FFUSecurityHeader = &(FFU->FFUSecurityHeader);
    struct FFUImageHeader_t * FFUImageHeader = &(FFU->FFUImageHeader);
    struct FFUStoreHeader_t * FFUStoreHeader = &(FFU->FFUStoreHeader);

    result = fread(FFUSecurityHeader, sizeof(char), sizeof(struct FFUSecurityHeader_t), fd);
    if(result != sizeof(struct FFUSecurityHeader_t))
    {
        dbg(LOG_ERROR, "ERROR! Couldn't properly read out FFUSecurityHeader\n");
        ExitAndShowLog(1);
    }

    hashedChunkSizeInKB = FFUSecurityHeader->dwChunkSizeInKb;
    hashedChunkSizeInBytes = hashedChunkSizeInKB*1024;

    //stride past header, catalog and hash table
    currentFileOffset = sizeof(struct FFUSecurityHeader_t) + FFUSecurityHeader->dwCatalogSize + FFUSecurityHeader->dwHashTableSize;
    //adjust padding
    currentFileOffset = AdjustPadding(hashedChunkSizeInBytes, currentFileOffset);
    //Seek to file offset
    fseek(fd, currentFileOffset, SEEK_SET);

    result = fread(FFUImageHeader, sizeof(char), sizeof(struct FFUImageHeader_t), fd);
    if(result != sizeof(struct FFUImageHeader_t))
    {
        dbg(LOG_ERROR, "ERROR! Couldn't properly read out FFUImageHeader\n");
        ExitAndShowLog(1);
    }

    currentFileOffset = currentFileOffset + sizeof(struct FFUImageHeader_t) + FFUImageHeader->ManifestLength;
    currentFileOffset = AdjustPadding(hashedChunkSizeInBytes, currentFileOffset);
    fseek(fd, currentFileOffset, SEEK_SET);

    result = fread(FFUStoreHeader, sizeof(char), sizeof(struct FFUStoreHeader_t), fd);
    if(result != sizeof(struct FFUStoreHeader_t))
    {
        dbg(LOG_ERROR, "ERROR! Couldn't properly read out FFUStoreHeader\n");
        ExitAndShowLog(1);
    }

    currentFileOffset += sizeof(struct FFUStoreHeader_t);
    currentFileOffset = AdjustPadding(sizeof(struct FFUBlockDataEntry_t), currentFileOffset);

    if (FFUStoreHeader->dwValidateDescriptorLength != 0)
        FFU->ValidationEntries = (char *)malloc(FFUStoreHeader->dwValidateDescriptorLength);
    if (FFUStoreHeader->dwWriteDescriptorLength != 0)
        FFU->BlockDataEntries = (struct FFUBlockDataEntry_t *)malloc(FFUStoreHeader->dwWriteDescriptorLength);

    if( (FFUStoreHeader->dwValidateDescriptorLength < 1000000 ) && 
      (FFUStoreHeader->dwWriteDescriptorLength < 1000000) && 
      (FFUStoreHeader->dwWriteDescriptorCount < 0x10000000) )
    {
        if ((FFUStoreHeader->dwValidateDescriptorLength > 0 && FFU->ValidationEntries == NULL) || 
            (FFUStoreHeader->dwWriteDescriptorLength > 0 && FFU->BlockDataEntries == NULL))
        {
            dbg(LOG_ERROR, "ERROR! Failed to allocate space for validation entries or blockdataentries\n");
            ExitAndShowLog(1);
        }

        if(FFUStoreHeader->dwValidateDescriptorLength > 0)
        {
            fseek(fd, currentFileOffset, SEEK_SET);
            if(fread(FFU->ValidationEntries, sizeof(char), FFUStoreHeader->dwValidateDescriptorLength, fd)
                != (size_t)FFUStoreHeader->dwValidateDescriptorLength)
            {
                dbg(LOG_ERROR, "ERROR! Failed to read validation entries\n");
                ExitAndShowLog(1);
            }
            currentFileOffset += FFUStoreHeader->dwValidateDescriptorLength;
        }

        if(FFUStoreHeader->dwWriteDescriptorLength > 0)
        {
            fseek(fd, currentFileOffset, SEEK_SET);
            if(fread(FFU->BlockDataEntries, sizeof(char), FFUStoreHeader->dwWriteDescriptorLength, fd)
                != (size_t)FFUStoreHeader->dwWriteDescriptorLength)
            {
                dbg(LOG_ERROR, "ERROR! Failed to read block data entries\n");
                ExitAndShowLog(1);
            }
            currentFileOffset += FFUStoreHeader->dwWriteDescriptorLength;
        }

        currentFileOffset = AdjustPadding(hashedChunkSizeInBytes, currentFileOffset);
        FFU->PayloadDataStart = currentFileOffset;
    }
}

void ReadFFUGPT(struct FFUImage_t * FFU, FILE * fd)
{
    size_t result;
    uint64_t readOffset = FFU->PayloadDataStart + 
                        FFU->FFUStoreHeader.dwFinalTableIndex*FFU->FFUStoreHeader.dwBlockSizeInBytes;
    FFU->GptEntries = (struct gpt_entry_t *)malloc(sizeof(struct gpt_entry_t)*128);
    fseek(fd, readOffset, SEEK_SET);

    result = fread(&(FFU->GptProtectiveMBR), sizeof(char),sizeof(FFU->GptProtectiveMBR), fd);
    if(result != sizeof(FFU->GptProtectiveMBR))
    {
        dbg(LOG_ERROR, "ERROR!, couldn't properly read out the protective MBR\n");
        ExitAndShowLog(1);
    }

    result = fread(&(FFU->GptHeader), sizeof(char), sizeof(&(FFU->GptHeader)), fd);
    if(result != sizeof(&(FFU->GptHeader)))
    {
        dbg(LOG_ERROR, "ERROR!, couldn't properly read out the Gpt Header\n");
        ExitAndShowLog(1);
    }

    result = fread(FFU->GptEntries, sizeof(char), sizeof(struct gpt_entry_t)*128, fd);
    if(result != sizeof(struct gpt_entry_t)*128)
    {
        dbg(LOG_ERROR, "ERROR!, couldn't properly read gpt entries\n");
        ExitAndShowLog(1);
    }
}

void DumpFFURawProgram(struct FFUImage_t * FFU)
{
    boolean bPendingData = FALSE;
    uint32_t bytesRead = 0;
    uint32_t dwNextBlockIndex = 0;
    uint32_t dwBlockOffset = 0;
    uint32_t dwStartBlock = 0;
    struct FFUBlockDataEntry_t * BlockDataEntriesPtr = FFU->BlockDataEntries;
    int64_t diskOffset = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    struct FFUStoreHeader_t * FFUStoreHeader = &(FFU->FFUStoreHeader);

    for (i = 0; i < FFUStoreHeader->dwWriteDescriptorCount; i++) 
    {
        for (j = 0; j < BlockDataEntriesPtr->dwLocationCount; j++)
        {
            if( BlockDataEntriesPtr->rgDiskLocations[j].dwDiskAccessMethod == DISK_BEGIN  ) 
            {
                // If this block does not follow the previous block create a new chunk 
                if( BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex != dwNextBlockIndex ) 
                {
                    // Add this entry to the rawprogram0.xml
                    if (bPendingData) 
                    {
                        CreateFFUXMLTag(FFU,(FFU->PayloadDataStart + dwStartBlock*FFUStoreHeader->dwBlockSizeInBytes)/fh.attrs.SECTOR_SIZE_IN_BYTES,
                                    (diskOffset / fh.attrs.SECTOR_SIZE_IN_BYTES),
                                    (dwBlockOffset - dwStartBlock) * 128 * 2);

                    }
                    diskOffset = (uint64_t)BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex*(uint64_t)FFUStoreHeader->dwBlockSizeInBytes;
                    dwStartBlock = dwBlockOffset;
                    bPendingData = TRUE;
                }
    
                dwNextBlockIndex = BlockDataEntriesPtr->rgDiskLocations[j].dwBlockIndex+1;
            } 
            else if( BlockDataEntriesPtr->rgDiskLocations[j].dwDiskAccessMethod == DISK_END ) 
            {
                // Add this entry to the rawprogram0.xml
                if (bPendingData) 
                {
                    CreateFFUXMLTag(FFU,(FFU->PayloadDataStart + dwStartBlock*FFUStoreHeader->dwBlockSizeInBytes)/fh.attrs.SECTOR_SIZE_IN_BYTES,
                                    (diskOffset / fh.attrs.SECTOR_SIZE_IN_BYTES),
                                    (i - dwStartBlock) * 128 * 2);
                }
    
                diskOffset = (int64_t)-1 * (int64_t)FFUStoreHeader->dwBlockSizeInBytes;  // End of disk is -1
    
                CreateFFUXMLTag(FFU,(FFU->PayloadDataStart + dwBlockOffset*FFUStoreHeader->dwBlockSizeInBytes)/fh.attrs.SECTOR_SIZE_IN_BYTES,
                                    (diskOffset / fh.attrs.SECTOR_SIZE_IN_BYTES),
                                    FFUStoreHeader->dwBlockSizeInBytes/fh.attrs.SECTOR_SIZE_IN_BYTES);
                bPendingData = FALSE;
            }
        }
        // Increment our offset by number of blocks
        if (BlockDataEntriesPtr->dwLocationCount > 0) 
        {
            BlockDataEntriesPtr = (struct FFUBlockDataEntry_t *)((uint32_t)BlockDataEntriesPtr + (sizeof(struct FFUBlockDataEntry_t)+(BlockDataEntriesPtr->dwLocationCount - 1)*sizeof(struct FFUDiskLocation_t)));
        }
        else 
        {
            BlockDataEntriesPtr = (struct FFUBlockDataEntry_t *)((uint32_t)BlockDataEntriesPtr + sizeof(struct FFUBlockDataEntry_t));
        }
        dwBlockOffset++;
    }

    if( bPendingData ) 
    {
        CreateFFUXMLTag(FFU,(FFU->PayloadDataStart + dwStartBlock*FFUStoreHeader->dwBlockSizeInBytes)/fh.attrs.SECTOR_SIZE_IN_BYTES,
                                    (diskOffset / fh.attrs.SECTOR_SIZE_IN_BYTES),
                                    (dwBlockOffset - dwStartBlock) * 128 * 2);
    }

}

void CreateFFUXMLTag(struct FFUImage_t * FFU, uint64_t FileOffset, int64_t StartSector, uint64_t NumSectors)
{
    InitBufferWithXMLHeader(tx_buffer, sizeof (tx_buffer));
    AppendToBuffer(tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);

    AppendToBuffer(tx_buffer, "<program ", FIREHOSE_TX_BUFFER_SIZE); //the program tag

    sprintf(temp_buffer, "filename=\"%s\" ", FFU->filename);

    AppendToBuffer(tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    sprintf(temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", FFU->sector_size_in_bytes); 
    AppendToBuffer(tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    sprintf(temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", NumSectors);
    AppendToBuffer(tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    sprintf(temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", FFU->physical_partition_number);
    AppendToBuffer(tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
    
    if (StartSector < 0)
        sprintf(temp_buffer, "start_sector=\"NUM_DISK_SECTORS%"SIZE_T_FORMAT"\" ", StartSector);
    else
        sprintf(temp_buffer, "start_sector=\"%"SIZE_T_FORMAT"\" ", StartSector);
    AppendToBuffer(tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    sprintf(temp_buffer, "file_sector_offset=\"%"SIZE_T_FORMAT"\" ", FileOffset);
    AppendToBuffer(tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    AppendToBuffer(tx_buffer, " />\n</data>", FIREHOSE_TX_BUFFER_SIZE);

    CopyString(XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen(tx_buffer), MAX_XML_SIZE, FIREHOSE_TX_BUFFER_SIZE);
    NumXMLFilesInTable++;
}

boolean TestIfWindowsFFU(FILE * fd)
{
    struct FFUSecurityHeader_t TempStruct;
    size_t result;

    result = fread(&TempStruct, sizeof(char), sizeof(struct FFUSecurityHeader_t), fd);
    if (result != sizeof(struct FFUSecurityHeader_t))
    {
        return FALSE;
    }

    fseek(fd, 0, SEEK_SET);
    if(!strncmp(TempStruct.signature, "SignedImage", 11))
    {
        return TRUE;
    }
    return FALSE;
}

/*====================================================
===================== END FFU FUNCTIONS===============
======================================================*/

int main (int argc, char *argv[])
{

    char *PathToSaveFiles = NULL;
    char szTemp[2048] = "19-2";
    int  szIndex = 0;
    SIZE_T  PacketLoc = 0, i, j, k;
    char c;
    char *FileToSendWithPath = NULL;
    FILE *fd;
    char build_month[2];
    SIZE_T MyLun = 0, MyStartSector = 0, MyNumSectors = 0, MyTrials = 10;
    char fh_loader_version[18] =
    {
        // Build the version information.
        __DATE__[9], __DATE__[10], '.',   // Year
        __DATE__[0], __DATE__[2], '.',    // Month
        __DATE__[4], __DATE__[5], '.',    // Day
        __TIME__[0], __TIME__[1], '.',    // Hour
        __TIME__[3], __TIME__[4], '.',    // minute
        __TIME__[6], __TIME__[7], 0,       // seconds
    };

    get_build_month (build_month);
    fh_loader_version[3] = build_month[0];
    fh_loader_version[4] = build_month[1];

    LoadBuffer (temp_buffer, ONE_MEGABYTE);
    //MyCopyFile("C:\\Users\\ahughes\\hello.txt","C:\\Users\\ahughes\\hello2.txt");
    //FindPartitionByLabel(0, "mom", "gpt_main0.bin");
    //exit(0);

    printf ("\nBinary build date: %s @ %s\n", __DATE__, __TIME__);
    printf ("Build version: %s\n", fh_loader_version);

    /*
      struct timeval time_start, time_end, AbsoluteTimeStart;
        double ElaspedTime;

      gettimeofday(&time_start, NULL);

        printf("\n");
        for(i=0;i<5;i++)
        {
            printf(".\n");
            sleep(1);
        }

      gettimeofday(&time_end, NULL);
        ElaspedTime = ReturnTimeInSeconds(&time_start,&time_end);
        printf("ElaspedTime = %f\n",ElaspedTime);
        exit(0);
    */
    //MyParseExpression(szTemp, 20, &i);
    //parseExpression(szTemp,&i);



    /* check argument count, print the usage help and quit */
    if (argc < 2)
    {
        print_usage (stderr);
        exit (0);
    }

    InitAttributes(); // all the fh.attrs

    // This must be before logging is brought up, since I need to know CWD exists
    if (GETCWD (cwd, sizeof (cwd) ) != NULL)
    {
        i = strlen (cwd);
        cwd[i]  = SLASH;
        cwd[i + 1] = '\0';
    }
    else
    {
        printf ("ERROR: Calling getcwd() failed");
        exit (0); // there is no log to show yet
    }


    PromptUser = 0; // turn off here. So that any mistake at this point just exits


    // PASS 1 OF COMMAND LINE OPTIONS
    //    PURPOSE: Make sure options are valid

    for (i = 1; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
    {
        getoptarg (i, argv, MyOpt, sizeof (MyOpt), MyArg, sizeof (MyArg) );

        if (PartOfCommandLineOptions (MyOpt) == 0)
        {
            ShowCommandLineOptions();

            dbg (LOG_ERROR, "Command line argument '%s' is not recognized as a valid option", MyOpt);
            ExitAndShowLog (1);
        }
    }

    // Quick sanity check
    for (i = 1; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
    {
        getoptarg (i, argv, MyOpt, sizeof (MyOpt), MyArg, sizeof (MyArg) );

        if (MyOpt[strlen (MyOpt) - 1] == '=')
        {
            // did they provide an argument?
            if (strlen (MyArg) == 0)
            {
                dbg (LOG_ERROR, "Missing argument for --%s", MyOpt);
                ExitAndShowLog (1);
            }
        }
    }

    for (i = 1; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
    {
        getoptarg (i, argv, MyOpt, sizeof (MyOpt), MyArg, sizeof (MyArg) );
        //dbg(LOG_INFO, "Looks like opt='%s' and arg='%s'\n\n",MyOpt,MyArg);

        if (strncmp (MyOpt, "porttracename=", MAX (strlen (MyOpt), strlen ("porttracename") ) ) == 0)
        {
            dbg (LOG_INFO, "User changed '%s' to '%s'\n", PortTraceName, MyArg);

            if ( CopyString (PortTraceName, MyArg, 0, 0, strlen (MyArg), sizeof (PortTraceName), strlen (MyArg) )  == 0)
            {
                dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into PortTraceName", MyArg, strlen (MyArg) );
                ExitAndShowLog (1);
            }

            continue;
        }

        else if (strncmp(MyOpt, "mainoutputdir=", MAX(strlen(MyOpt), strlen("mainoutputdir="))) == 0)
        {
          strcpy(MainOutputDir, MyArg);
          if (mkdir(MainOutputDir, 0777) == -1)
          {
            dbg(LOG_INFO, "Already exists");
          }
          else
          {
            dbg(LOG_INFO, "Created successfully");
          }
        }
    }

    InitLogging();    // Init Logging


    szIndex += sprintf (&szTemp[szIndex], "FH_LOADER WAS CALLED EXACTLY LIKE THIS\n************************************************\n");

    for (i = 0; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
        szIndex += sprintf (&szTemp[szIndex], "%s ", argv[i]);

    szIndex += sprintf (&szTemp[szIndex], "\n************************************************\n");
    dbg (LOG_INFO, szTemp);
    szIndex = 0;


    dbg (LOG_DEBUG, "Binary build date: %s @ %s\n", __DATE__, __TIME__);
    dbg (LOG_DEBUG, "Build Version: %s\n", fh_loader_version);
    dbg (LOG_INFO, "Current working dir (cwd): %s", cwd);
    dbg (LOG_INFO, "Showing network mappings to allow debugging");

#ifdef _MSC_VER // i.e. if compiling under Windows

    fg = _popen ("net use", "r");

    while (fgets (temp_buffer, sizeof (temp_buffer), fg) != 0)
    {
        dbg (LOG_DEBUG, "%s", temp_buffer);
    }

    _pclose (fg);

#endif


    PromptUser = 1; // this turns on here, user can turn off below with --noprompt

    // PASS 2 OF COMMAND LINE OPTIONS
    //    PURPOSE: Options that must be first, such as search_paths to allow finding of files possibly passed as other command line options
    //         erasefirst and wipefirst are flags that affect the <program> tag by adding erase/wipe prior to programming
    //               so if user used the --sendfile option, and --wipefirst I need to know

    for (i = 1; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
    {
        getoptarg (i, argv, MyOpt, sizeof (MyOpt), MyArg, sizeof (MyArg) );
        //dbg(LOG_INFO, "Looks like opt='%s' and arg='%s'\n\n",MyOpt,MyArg);

        if ( strncmp (MyOpt, "memoryname=", MAX (strlen (MyOpt), strlen ("memoryname=") ) ) == 0 )
        {
            if (MyArg[0] == 'u' || MyArg[0] == 'U')
            {
                if ( CopyString (fh.attrs.MemoryName, "ufs", 0, 0, strlen ("ufs"), sizeof (fh.attrs.MemoryName), sizeof ("ufs") )  == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.MemoryName", "ufs", strlen ("ufs") );
                    ExitAndShowLog (1);
                }
            }
            else if (MyArg[0] == 'n')
            {
                if ( CopyString (fh.attrs.MemoryName, "nand", 0, 0, strlen ("nand"), sizeof (fh.attrs.MemoryName), sizeof ("nand") )  == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.MemoryName", "nand", strlen ("nand") );
                    ExitAndShowLog (1);
                }
            }
            else if (MyArg[0] == 's')
            {
                if ( CopyString (fh.attrs.MemoryName, "spinor", 0, 0, strlen ("spinor"), sizeof (fh.attrs.MemoryName), sizeof ("spinor") )  == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.MemoryName", "spinor", strlen ("spinor") );
                    ExitAndShowLog (1);
                }
            }
            else
            {
                if ( CopyString (fh.attrs.MemoryName, "emmc", 0, 0, strlen ("emmc"), sizeof (fh.attrs.MemoryName), sizeof ("emmc") )  == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.MemoryName", "emmc", strlen ("emmc") );
                    ExitAndShowLog (1);
                }

            }

            if (fh.attrs.MemoryName[0] == 'u' || fh.attrs.MemoryName[0] == 's')
            {
                fh.attrs.SECTOR_SIZE_IN_BYTES = 4096;
                SectorSizeInBytes       = 4096;
            }
            else //if(fh.attrs.MemoryName[0]=='e')
            {
                fh.attrs.SECTOR_SIZE_IN_BYTES = 512;
                SectorSizeInBytes       = 512;
            }

            continue;
        }

        if (strncmp (MyOpt, "digestsperfilename=", MAX (strlen (MyOpt), strlen ("digestsperfilename") ) ) == 0)
        {
            dbg (LOG_INFO, "User changed '%s' to '%s'\n", DigestsPerFileName, MyArg);

            if (CopyString (DigestsPerFileName, MyArg, 0, 0, strlen (MyArg), sizeof (DigestsPerFileName), strlen (MyArg) ) == 0)
            {
                dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into DigestsPerFileName", MyArg, strlen (MyArg) );
                ExitAndShowLog (1);
            }

            continue;
        }
        else if ( strncmp (MyOpt, "noprompt", MAX (strlen (MyOpt), strlen ("noprompt") ) ) == 0)
        {
            PromptUser  = 0; // will no longer ask user, will instead just do the default
            continue;
        }
        else if (strncmp (MyOpt, "convertprogram2read", MAX (strlen (MyOpt), strlen ("convertprogram2read") ) ) == 0)
        {
            ConvertProgram2Read = 1;
        }
        else if (strncmp (MyOpt, "erase=", MAX (strlen (MyOpt), strlen ("erase=") ) ) == 0)
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<erase ", FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SectorSizeInBytes);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "physical_partition_number=\"%d\" ", atoi (MyArg) );
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        } // end of nop

        if ( strncmp (MyOpt, "files=", MAX (strlen (MyOpt), strlen ("files=") ) ) == 0 )
        {
            // MyArg is split on commas, results into filter_files[MAX_XML_FILES][MAX_PATH_SIZE]
            num_filter_files = SplitStringOnCommas (MyArg, sizeof (MyArg), filter_files, num_filter_files, MAX_XML_FILES, MAX_PATH_SIZE);

            filter_files[num_filter_files][0] = '\0'; // j is how many strings were valid, make sure last one in table is null

            continue;
        }

        if ( strncmp (MyOpt, "notfiles=", MAX (strlen (MyOpt), strlen ("notfiles=") ) ) == 0 )
        {
            // MyArg is split on commas, results into filter_not_files[MAX_XML_FILES][MAX_PATH_SIZE]
            num_filter_not_files = SplitStringOnCommas (MyArg, sizeof (MyArg), filter_not_files, num_filter_not_files, MAX_XML_FILES, MAX_PATH_SIZE);

            filter_not_files[num_filter_not_files][0] = '\0'; // j is how many strings were valid, make sure last one in table is null

            continue;
        }
    }

    for (i = 1; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
    {
        getoptarg (i, argv, MyOpt, sizeof (MyOpt), MyArg, sizeof (MyArg) );
        //dbg(LOG_INFO, "Looks like opt='%s' and arg='%s'\n\n",MyOpt,MyArg);

        if ( strncmp (MyOpt, "search_path=", MAX (strlen (MyOpt), strlen ("search_path=") ) ) == 0 )
        {
            // MyArg is split on commas, results into search_path[MAX_XML_FILES][MAX_PATH_SIZE]
            num_search_paths = SplitStringOnCommas (MyArg, sizeof (MyArg), search_path, num_search_paths, MAX_XML_FILES, MAX_PATH_SIZE);

            search_path[num_search_paths][0] = '\0'; // j is how many strings were valid, make sure last one in table is null

            CleanseSearchPaths();
            continue;
        }
        else if (strncmp (MyOpt, "firmwarewrite", MAX (strlen (MyOpt), strlen ("firmwarewrite") ) ) == 0)
        {
            ConvertProgram2Firmware = 1;
            fh.attrs.SECTOR_SIZE_IN_BYTES = 1;
            SectorSizeInBytes = 1;
        }
        else if (strncmp (MyOpt, "sectorsizeinbytes=", MAX (strlen (MyOpt), strlen ("sectorsizeinbytes=") ) ) == 0)
        {
            fh.attrs.SECTOR_SIZE_IN_BYTES = atoi (MyArg);
            SectorSizeInBytes       = atoi (MyArg);
            dbg (LOG_DEBUG, "User set SECTOR_SIZE_IN_BYTES to %ld\n", fh.attrs.SECTOR_SIZE_IN_BYTES);
            continue;
        }
        else if ( strncmp (MyOpt, "contentsxml=", MAX (strlen (MyOpt), strlen ("contentsxml=") ) ) == 0 )
        {
            CopyString (ContentsXMLFilename, MyArg, 0, 0, strlen (MyArg), MAX_STRING_SIZE, MAX_XML_FILE_SIZE);
        }
        else if ( strncmp (MyOpt, "flavor=", MAX (strlen (MyOpt), strlen ("flavor=") ) ) == 0 )
        {
            CopyString (flavor, MyArg, 0, 0, strlen (MyArg), MAX_STRING_SIZE, MAX_XML_FILE_SIZE);
        }
        else if ( strncmp (MyOpt, "createconfigxml", MAX (strlen (MyOpt), strlen ("createconfigxml") ) ) == 0 )
        {
            createconfigxml = 1;
            Simulate = 1;
        }
        else if ( strncmp (MyOpt, "flattenbuildto=", MAX (strlen (MyOpt), strlen ("flattenbuildto=") ) ) == 0 )
        {
            if ( CopyString (flattenbuildto, MyArg, 0, 0, strlen (MyArg), MAX_STRING_SIZE, sizeof (MyArg) )  == 0)
            {
                dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildto", MyArg, strlen (MyArg) );
                ExitAndShowLog (1);
            }

            if ( flattenbuildto[ strlen (flattenbuildto) ] == '\0' )
            {
                flattenbuildto[ strlen (flattenbuildto) + 1 ] = '\0';
                flattenbuildto[ strlen (flattenbuildto)]   = SLASH;
            }

            if (!HasAPathCharacter (flattenbuildto, strlen (flattenbuildto) ) )
            {
                // to be here means user entered a relative path, i.e. something like path2\path3 instead of c:\path1\path2\path3
                CopyString (temp_buffer, cwd, 0, 0, strlen (cwd), MAX_PATH_SIZE, MAX_PATH_SIZE);
                CopyString (temp_buffer, flattenbuildto, strlen (temp_buffer), 0, strlen (flattenbuildto), MAX_PATH_SIZE, MAX_PATH_SIZE);
                dbg (LOG_INFO, "'%s' changed to", flattenbuildto);
                CopyString (flattenbuildto, temp_buffer, 0, 0, strlen (temp_buffer), MAX_PATH_SIZE, MAX_PATH_SIZE);
                dbg (LOG_INFO, "this '%s'", flattenbuildto);
            }



            dbg (LOG_INFO, "Trying to create directory for --flattenbuildto='%s'", flattenbuildto);

            if (mkdir (flattenbuildto, 0777) == -1)
                dbg (LOG_INFO, "Already exists");
            else
                dbg (LOG_INFO, "Created successfully");


            // user provided this path, therefore save files into it
            memset (flattenbuildvariant, 0x0, sizeof (flattenbuildvariant) );

            if ( CopyString (flattenbuildvariant, flattenbuildto, strlen (flattenbuildvariant), 0, strlen (flattenbuildto), sizeof (flattenbuildvariant), sizeof (flattenbuildto) )  == 0)
            {
                dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildvariant", flattenbuildto, strlen (flattenbuildto) );
                ExitAndShowLog (1);
            }

            if ( CopyString (flattenbuildvariant, fh.attrs.MemoryName, strlen (flattenbuildvariant), 0, strlen (fh.attrs.MemoryName), sizeof (flattenbuildvariant), strlen (fh.attrs.MemoryName) )  == 0)
            {
                dbg (LOG_ERROR, "Failed to copy fh.attrs.MemoryName string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildvariant", fh.attrs.MemoryName, strlen (fh.attrs.MemoryName) );
                ExitAndShowLog (1);
            }

            if ( CopyString (flattenbuildvariant, "\\", strlen (flattenbuildvariant), 0, strlen ("\\"), sizeof (flattenbuildvariant), strlen ("\\") )  == 0)
            {
                dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildvariant", "\\", strlen ("\\") );
                ExitAndShowLog (1);
            }

            dbg (LOG_INFO, "Trying to create directory '%s' since fh.attrs.MemoryName is '%s'", flattenbuildvariant, fh.attrs.MemoryName);

            if (mkdir (flattenbuildvariant, 0777) == -1)
                dbg (LOG_INFO, "Already exists");
            else
                dbg (LOG_INFO, "Created successfully");


            dbg (LOG_DEBUG, "User wants FLATTEN BUILD - This will be a SIMULATE - Not reading/writing from a port\n");
            Simulate    = 1;
            FlattenBuild  = 1;
            createconfigxml = 1;
            AllowReset    = 0; // won't send <power DelayInSeconds="2" value="reset" /> anymore
        }
        else if (strncmp (MyOpt, "wipefirst", MAX (strlen (MyOpt), strlen ("wipefirst") ) ) == 0)
        {
            FILE * fTemp;
            char MyBufferOfZeros[16 * 1024] = {0};
            dbg (LOG_DEBUG, "User wants wipefirst - Will write zeros for the first 16KB of every <program> tag\n");
            wipefirst = 1;
            // Need to create the 16KB file of zeros
            fTemp = ReturnFileHandle (WipeFirstFileName, strlen (WipeFirstFileName), "wb"); // will exit if not successful
            fwrite (MyBufferOfZeros, sizeof (MyBufferOfZeros), 1, fTemp);
            fclose (fTemp);
            fTemp = NULL;
            continue;
        }
        else if (strncmp (MyOpt, "erasefirst", MAX (strlen (MyOpt), strlen ("erasefirst") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants erasefirst - Will issue <erase> tag before every <program> tag\n");
            erasefirst = 1;

            if (fh.attrs.MemoryName[0] == 'n' || fh.attrs.MemoryName[0] == 'N')
            {
                // looks like they chose nand || NAND
            }
            else
            {
                dbg (LOG_ERROR, "\n\nIt looks like you chose --erasefirst with MemoryName='%s'.\nThis is a --memoryname=NAND feature!!"
                     "\n\n\tWhat you are asking to do here is issue an <erase> command for every <program> tag"
                     "\n\tBut on '%s' this will have the effect of erasing the entire device *every time* for each <program> tag.\n\tAt a high level this means only your last file will be programmed!!"
                     "\n\n\nYou don't want this feature and you are looking for either --wipefirst or --erase\n\n"
                     "\n\n\t\tSomething like:    fh_loader.exe --erase --port= etc etc\n\n"
                     "\n\n\t\t            Or:    fh_loader.exe --wipefirst --port= etc etc\n\n"
                     , fh.attrs.MemoryName, fh.attrs.MemoryName);
                ExitAndShowLog (1);
            }

            continue;
        }
    }



    ParseContentsXML (ContentsXMLFilename); // "\\\\sundae\\builds664\\PROD\\M8994AAAAANLYD1024.4\\contents.xml"

    // add all the search paths
    for (i = 0; i < NumContensXML; i++)
    {
        // StorageType; // 0=unknown, 'e'=emmc,'u'=ufs,'n'=nand
        // FileType;  // 0=unknown,'r'=<partition_file (rawprogram),'p'=<partition_patch_file (patch)

        if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0 )
        {
            if (!AlreadyHaveThisPath (ContensXML[i].Path) )
            {
                if ( CopyString (search_path[num_search_paths], ContensXML[i].Path, 0, 0, strlen (ContensXML[i].Path), MAX_PATH_SIZE, MAX_PATH_SIZE)  == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy string '%s' into search_path[%i]", ContensXML[i].Path, num_search_paths);
                    return 1;
                }

                num_search_paths++;
            }
        }
    }

    if (NumContensXML > 0)
    {
        // to be here means ParseContentsXML(ContentsXMLFilename) returned something, so fill in the rawprograms and search_paths etc
        for (i = 0; i < NumContensXML; i++)
        {
            if ( ContensXML[i].FileType == 'r') // && ContensXML[i].StorageType==fh.attrs.MemoryName[0])
            {
                // this is a rawprogram file
                if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0 )
                {

                    //FILE *fTemp = ReturnFileHandle(ContensXML[i].Filename, MAX_PATH_SIZE, "r");   // will exit if not successful
                    //fclose(fTemp);
                    if ( CopyString (XMLFileTable[num_xml_files_to_send], ContensXML[i].Path, 0, 0, strlen (ContensXML[i].Path), MAX_PATH_SIZE, MAX_PATH_SIZE) == 0)
                    {
                        dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into XMLFileTable[%i]", ContensXML[i].Filename, strlen (ContensXML[i].Filename), num_xml_files_to_send);
                        return 1;
                    }

                    if ( CopyString (XMLFileTable[num_xml_files_to_send], ContensXML[i].Filename, strlen (ContensXML[i].Path), 0, strlen (ContensXML[i].Filename), MAX_PATH_SIZE, MAX_ATTR_NAME_SIZE) == 0)
                    {
                        dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into XMLFileTable[%i]", ContensXML[i].Filename, strlen (ContensXML[i].Filename), num_xml_files_to_send);
                        return 1;
                    }

                    num_xml_files_to_send++;
                }
            } // end if(fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType==0 )
        }

        OpenAndStoreAllXMLFiles();
        num_xml_files_to_send = 0;

        for (i = 0; i < NumContensXML; i++)
        {
            // now load the patch files
            if ( ContensXML[i].FileType == 'p' ) // && ContensXML[i].StorageType==fh.attrs.MemoryName[0])
            {
                if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0 )
                {
                    // this is a patch file
                    //FILE *fTemp = ReturnFileHandle(ContensXML[i].Filename, MAX_PATH_SIZE, "r");   // will exit if not successful
                    //fclose(fTemp);
                    if ( CopyString (XMLFileTable[num_xml_files_to_send], ContensXML[i].Path, 0, 0, strlen (ContensXML[i].Path), MAX_PATH_SIZE, MAX_PATH_SIZE) == 0)
                    {
                        dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into XMLFileTable[%i]", ContensXML[i].Filename, strlen (ContensXML[i].Filename), num_xml_files_to_send);
                        return 1;
                    }

                    if ( CopyString (XMLFileTable[num_xml_files_to_send], ContensXML[i].Filename, strlen (ContensXML[i].Path), 0, strlen (ContensXML[i].Filename), MAX_PATH_SIZE, MAX_ATTR_NAME_SIZE) == 0)
                    {
                        dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into XMLFileTable[%i]", ContensXML[i].Filename, strlen (ContensXML[i].Filename), num_xml_files_to_send);
                        return 1;
                    }

                    num_xml_files_to_send++;
                }
            } // end if(fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType==0 )
        }

        OpenAndStoreAllXMLFiles();
        num_xml_files_to_send = 0;

    } // end of if(NumContensXML>0)




    // PASS 3 OF COMMAND LINE OPTIONS
    //    PURPOSE: Most command line options parsed here

    for (i = 1; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
    {
        getoptarg (i, argv, MyOpt, sizeof (MyOpt), MyArg, sizeof (MyArg) );
        //dbg(LOG_INFO, "Looks like opt='%s' and arg='%s'\n\n",MyOpt,MyArg);

        if (strncmp (MyOpt, "port=", MAX (strlen (MyOpt), strlen ("port=") ) ) == 0)
        {
            if ( CopyString (port_name, MyArg, 0, 0, strlen (MyArg), sizeof (port_name), sizeof (MyArg) )  == 0)
            {
                dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into port_name", MyArg, strlen (MyArg) );
                ExitAndShowLog (1);
            }

            continue;
        }
        else if (strncmp (MyOpt, "loglevel=", MAX (strlen (MyOpt), strlen ("loglevel=") ) ) == 0)
        {
            k = atoi (MyArg);

            if (k == 0) // user wants silent
            {
                dbg (LOG_ALWAYS, "User requested *minimal* logging with --loglevel=0");
                VerboseLevel  = LOG_ALWAYS; // LOG_ALWAYS or lower shown (i.e. LOG_ERROR)
            }
            else if (k == 2) // user wants superverbose <-- log will show HEX dump of RAW packets too BIG LOG
            {
                dbg (LOG_ALWAYS, "User requested *verbose* logging with --loglevel=2");
                VerboseLevel  = LOG_DEBUG;  // Everything is shown
                createcommandtrace = 1;
            }
            else if (k == 3) // user wants superverbose <-- log will show HEX dump of RAW packets too BIG LOG
            {
                dbg (LOG_ALWAYS, "User requested *super verbose* logging with --loglevel=3");
                VerboseLevel  = LOG_DEBUG;  // Everything is shown
                PrettyPrintRawPacketsToo = 1;
                createcommandtrace = 1;
            }
            else  // user wants normal
            {
                dbg (LOG_ALWAYS, "User requested *semi-verbose* logging with --loglevel=1 - This is default");
                VerboseLevel  = LOG_INFO; // Everything is shown
            }

            continue;
        }
        else if (strncmp (MyOpt, "interactive=", MAX (strlen (MyOpt), strlen ("interactive=") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants INTERACTIVE\n");
            Interactive = atoi (MyArg);

            if (Interactive > 2)
                Interactive = 2;  // 1 = XML files only, 2 means RAWDATA also

            continue;
        }
        else if (strncmp (MyOpt, "lun=", MAX (strlen (MyOpt), strlen ("lun=") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User is specifying lun=%s\n", MyArg);
            MyLun = (SIZE_T) atoi (MyArg);
            dbg (LOG_DEBUG, "MyLun=%"SIZE_T_FORMAT"\n", MyLun);

            if (MyLun > 7)
                dbg (LOG_WARN, "In most cases devices only have LUN 0,1,2,3,4,5,6,7. You have specified to use LUN %"SIZE_T_FORMAT" and this might fail\n\n", MyLun);

            continue;
        }
        else if (strncmp (MyOpt, "num_sectors=", MAX (strlen (MyOpt), strlen ("num_sectors=") ) ) == 0)
        {
            boolean num_conversion;

            dbg (LOG_DEBUG, "User is specifying num_sectors=%s\n", MyArg);

            MyNumSectors = stringToNumber ( (const char *) MyArg, &num_conversion);

            if (FALSE == num_conversion)
            {
                dbg (LOG_INFO, "Call to stringToNumber failed for MyNumSectors with value '%s'", MyArg);
                ExitAndShowLog (1);
            }

            dbg (LOG_DEBUG, "MyNumSectors=%"SIZE_T_FORMAT"\n", MyNumSectors);
            continue;
        }
        else if (strncmp (MyOpt, "trials=", MAX (strlen (MyOpt), strlen ("trials=") ) ) == 0)
        {
            boolean num_conversion;

            dbg (LOG_DEBUG, "User is specifying trials=%s\n", MyArg);

            MyTrials = stringToNumber ( (const char *) MyArg, &num_conversion);

            if (FALSE == num_conversion)
            {
                dbg (LOG_INFO, "Call to stringToNumber failed for MyTrials with value '%s'", MyArg);
                ExitAndShowLog (1);
            }

            dbg (LOG_DEBUG, "MyTrials=%"SIZE_T_FORMAT"\n", MyTrials);
            continue;
        }
        else if (strncmp (MyOpt, "start_sector=", MAX (strlen (MyOpt), strlen ("start_sector=") ) ) == 0)
        {
            boolean num_conversion;

            dbg (LOG_DEBUG, "User is specifying start_sector=%s\n", MyArg);

            MyStartSector = stringToNumber ( (const char *) MyArg, &num_conversion);

            if (FALSE == num_conversion)
            {
                dbg (LOG_INFO, "Call to stringToNumber failed for MyStartSector with value '%s'", MyArg);
                ExitAndShowLog (1);
            }

            dbg (LOG_DEBUG, "MyStartSector=%"SIZE_T_FORMAT"\n", MyStartSector);
            continue;
        }
        else if ( strncmp (MyOpt, "noreset", MAX (strlen (MyOpt), strlen ("noreset") ) ) == 0 )
        {
            AllowReset = 0; // won't send <power DelayInSeconds="2" value="reset" /> anymore
            continue;
        }
        else if ( strncmp (MyOpt, "reset", MAX (strlen (MyOpt), strlen ("reset") ) ) == 0 )
        {
            AllowReset = 1; // won't send <power DelayInSeconds="2" value="reset" /> anymore
            continue;
        }
        else if ( strncmp (MyOpt, "signeddigests=", MAX (strlen (MyOpt), strlen ("signeddigests=") ) ) == 0 )
        {
            CopyString (SignedDigestTableFilename, MyArg, 0, 0, strlen (MyArg), MAX_STRING_SIZE, MAX_XML_FILE_SIZE);
        }
        else if ( strncmp (MyOpt, "chaineddigests=", MAX (strlen (MyOpt), strlen ("chaineddigests=") ) ) == 0 )
        {
            CopyString (ChainedDigestTableFilename, MyArg, 0, 0, strlen (MyArg), MAX_STRING_SIZE, MAX_XML_FILE_SIZE);
        }
        else if ( strncmp (MyOpt, "setactivepartition=", MAX (strlen (MyOpt), strlen ("setactivepartition=") ) ) == 0 )
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<setbootablestoragedrive ", FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "value=\"%i\" ", atoi (MyArg) );
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "/>\n</data>\n", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, remove the \n

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        }
        else if ( strncmp (MyOpt, "getstorageinfo=", MAX (strlen (MyOpt), strlen ("getstorageinfo=") ) ) == 0 )
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<getstorageinfo ", FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "physical_partition_number=\"%i\" ", atoi (MyArg) );
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "/>\n</data>\n", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, remove the \n

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        }
        else if ( strncmp (MyOpt, "dontsorttags", MAX (strlen (MyOpt), strlen ("dontsorttags") ) ) == 0)
        {
            SortTags = 0; // will no longer ensure that <erase> is first and <patch> is last
            continue;
        }
        else if ( strncmp (MyOpt, "showdigestperpacket", MAX (strlen (MyOpt), strlen ("showdigestperpacket") ) ) == 0)
        {
            ShowDigestPerPacket = 1; // will show a SHA256 on all packets sent to target
            createcommandtrace  = 1;
            continue;
        }
        else if ( strncmp (MyOpt, "showdigestperfile", MAX (strlen (MyOpt), strlen ("showdigestperfile") ) ) == 0)
        {
            ShowDigestPerFile = 1; // will show a SHA256 per file
            fdp = ReturnFileHandle (DigestsPerFileName, MAX_PATH_SIZE, "w"); // will exit if not successful

            continue;
        }
        else if ( strncmp (MyOpt, "showpercentagecomplete", MAX (strlen (MyOpt), strlen ("showpercentagecomplete") ) ) == 0)
        {
            showpercentagecomplete = 1; // does a simulation first to get build size
            continue;
        }
        else if ( strncmp (MyOpt, "createcommandtrace", MAX (strlen (MyOpt), strlen ("createcommandtrace") ) ) == 0)
        {
            createcommandtrace = 1; // will show a SHA256 on all packets sent to target
            continue;
        }
        else if ( strncmp (MyOpt, "sendxml=", MAX (strlen (MyOpt), strlen ("sendxml=") ) ) == 0 )
        {
            // split on commas, store in XMLFileTable[MAX_XML_FILES][MAX_PATH_SIZE]
            num_xml_files_to_send = SplitStringOnCommas (MyArg, sizeof (MyArg), XMLFileTable, num_xml_files_to_send, MAX_XML_FILES, MAX_PATH_SIZE);

            XMLFileTable[num_xml_files_to_send][0] = '\0'; // j is how many strings were valid, make sure last one in table is null

            OpenAndStoreAllXMLFiles();

            num_xml_files_to_send = 0;

            continue;
        }
        else if (strncmp (MyOpt, "verbose", MAX (strlen (MyOpt), strlen ("verbose") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants FIREHOSE VERBOSE - Target will log much of what it is doing\n");
            Verbose = 1;
            continue;
        }
        else if (strncmp (MyOpt, "zlpawarehost=", MAX (strlen (MyOpt), strlen ("zlpawarehost=") ) ) == 0)
        {
            fh.attrs.ZlpAwareHost = atoi (MyArg);

            if (fh.attrs.ZlpAwareHost > 1)
                fh.attrs.ZlpAwareHost = 1;

            dbg (LOG_DEBUG, "User set ZLPAWAREHOST to %lld\n", fh.attrs.ZlpAwareHost);
            continue;
        }
        else if (strncmp (MyOpt, "verify_programming", MAX (strlen (MyOpt), strlen ("verify_programming") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants verify_programming\n");
            verify_programming = 1;
            continue;
        }
        else if (strncmp (MyOpt, "testvipimpact", MAX (strlen (MyOpt), strlen ("testvipimpact") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants testvipimpact\n");
            testvipimpact = 1;
            continue;
        }
        else if (strncmp (MyOpt, "simulate", MAX (strlen (MyOpt), strlen ("simulate") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants SIMULATION - Not reading/writing from a port\n");
            Simulate = 1;
            continue;
        }
        else if (strncmp (MyOpt, "createdigests", MAX (strlen (MyOpt), strlen ("createdigests") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants SIMULATION (CreateDigests) - Not reading/writing from a port\n");
            CreateDigests   = 1;
            Simulate      = 1;
            ShowDigestPerPacket = 1;
            continue;
        }
        else if (strncmp (MyOpt, "skipwrite", MAX (strlen (MyOpt), strlen ("skipwrite") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants skipwrite - Will not commit to flash. Use this to benchmark USB speed\n");
            fh.attrs.SkipWrite = 1;
            continue;
        }
        else if (strncmp (MyOpt, "skippatch", MAX (strlen (MyOpt), strlen ("skippatch") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants skippatch (LOAD_PATCH_PROGRAM_FILES=0) - Will issue any <patch commands\n");
            LOAD_PATCH_PROGRAM_FILES = 0;
            continue;
        }
        else if (strncmp (MyOpt, "skipstorageinit", MAX (strlen (MyOpt), strlen ("skipstorageinit") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants skipstorageinit - Will not initialize storage with <configure> i.e. handleConfigure()\n");
            fh.attrs.SkipStorageInit = 1;
            continue;
        }
        else if (strncmp (MyOpt, "readbogusdata", MAX (strlen (MyOpt), strlen ("readbogusdata") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants ReadBogusDataBeforeSendingConfigure - Will attempt to read 4KB of data before sending <configure> command\n");
            ReadBogusDataBeforeSendingConfigure = 1;
            continue;
        }

        else if (strncmp (MyOpt, "skipharddriveread", MAX (strlen (MyOpt), strlen ("skipharddriveread") ) ) == 0)
        {
            dbg (LOG_DEBUG, "User wants skipharddriveread - Will get filesizes from files, but data will be garbage\n");
            skipharddriveread = 1;
            continue;
        }
        else if (strncmp (MyOpt, "maxpayloadsizeinbytes=", MAX (strlen (MyOpt), strlen ("maxpayloadsizeinbytes=") ) ) == 0)
        {
            fh.attrs.MaxPayloadSizeToTargetInBytes = atoi (MyArg);
            dbg (LOG_DEBUG, "User set MaxPayloadSizeToTargetInBytes to %ld\n", fh.attrs.MaxPayloadSizeToTargetInBytes);
            continue;
        }
        else if (strncmp (MyOpt, "comportopentimeout=", MAX (strlen (MyOpt), strlen ("comportopentimeout=") ) ) == 0)
        {
            ComPortOpenTimeout = atof (MyArg);
            dbg (LOG_INFO, "User set ComPortOpenTimeout to %f\n", ComPortOpenTimeout);
            continue;
        }
        else if (strncmp (MyOpt, "maxdigesttablesizeinbytes=", MAX (strlen (MyOpt), strlen ("maxdigesttablesizeinbytes=") ) ) == 0)
        {
            fh.attrs.MaxDigestTableSizeInBytes = atoi (MyArg);
            dbg (LOG_DEBUG, "User set MaxDigestTableSizeInBytes to %ld\n", fh.attrs.MaxDigestTableSizeInBytes);
            MaxNumDigestsPerTable = fh.attrs.MaxDigestTableSizeInBytes / DigestSizeInBytes;
            continue;
        }
        else if (strncmp (MyOpt, "nop", MAX (strlen (MyOpt), strlen ("nop") ) ) == 0)
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<NOP value=\"ping\" ", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, Change NOP back to nop
            AppendToBuffer (tx_buffer, "/>\n</data>\n", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, remove the \n

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        } // end of nop

        else if (strncmp(MyOpt, "forceoverwrite", MAX(strlen(MyOpt), strlen("forceoverwrite"))) == 0)
        {
          ForceOverwrite = 1;
        }

    }


    OpenAndStoreAllXMLFiles();
    num_xml_files_to_send = 0;


    // PASS 4 OF COMMAND LINE OPTIONS
    //    PURPOSE: Options that need to be parsed last, such as getgptmainbackup because this issues a <read>
    //               and I need to know MemoryName, sectorsizeinbytes etc etc before creating this tag

    for (i = 1; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
    {
        getoptarg (i, argv, MyOpt, sizeof (MyOpt), MyArg, sizeof (MyArg) );

        if ( strncmp (MyOpt, "getgptmainbackup=", MAX (strlen (MyOpt), strlen ("getgptmainbackup=") ) ) == 0 )
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<read ", FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", fh.attrs.SECTOR_SIZE_IN_BYTES);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            if (fh.attrs.SECTOR_SIZE_IN_BYTES == 4096)
                sprintf (temp_buffer, "num_partition_sectors=\"6\" ");
            else
                sprintf (temp_buffer, "num_partition_sectors=\"34\" "); // 512 byte sector size

            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "filename=\"gpt_main%d.bin\" ", atoi (MyArg) );
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "physical_partition_number=\"%i\" ", atoi (MyArg) );
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "start_sector=\"0\" ");
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer (tx_buffer, "/>\n</data>\n", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, remove the \n

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }


            // Now the backup
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<read ", FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", fh.attrs.SECTOR_SIZE_IN_BYTES);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            if (fh.attrs.SECTOR_SIZE_IN_BYTES == 4096)
                sprintf (temp_buffer, "num_partition_sectors=\"5\" ");
            else
                sprintf (temp_buffer, "num_partition_sectors=\"33\" "); // 512 byte sector size

            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "filename=\"gpt_backup%d.bin\" ", atoi (MyArg) );
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "physical_partition_number=\"%i\" ", atoi (MyArg) );
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            if (fh.attrs.SECTOR_SIZE_IN_BYTES == 4096)
                sprintf (temp_buffer, "start_sector=\"NUM_DISK_SECTORS-5.\" ");
            else
                sprintf (temp_buffer, "start_sector=\"NUM_DISK_SECTORS-33.\" ");

            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer (tx_buffer, "/>\n</data>\n", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, remove the \n

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }


            continue;
        }
        else if (strncmp (MyOpt, "forcecontentsxmlpaths", MAX (strlen (MyOpt), strlen ("forcecontentsxmlpaths") ) ) == 0)
        {
            dbg (LOG_INFO, "User requested forcecontentsxmlpaths");
            forcecontentsxmlpaths = 1;
        }
        else if (strncmp (MyOpt, "verify_build", MAX (strlen (MyOpt), strlen ("verify_build") ) ) == 0)
        {
            dbg (LOG_INFO, "User requested verify_build, NOTE: This is to be used with --contentsxml=");
            verify_build = 1;
            Simulate   = 1;
            AllowReset   = 0; // won't send <power DelayInSeconds="2" value="reset" /> anymore
            //VerboseLevel  = LOG_DEBUG;
        }

        else if (strncmp (MyOpt, "benchmarkwrites", MAX (strlen (MyOpt), strlen ("benchmarkwrites") ) ) == 0)
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<benchmark test=\"TestWritePerformance\" trials=\"500\" TestWritePerformance=\"1\" ", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        } // end of nop

        else if (strncmp (MyOpt, "benchmarkreads", MAX (strlen (MyOpt), strlen ("benchmarkreads") ) ) == 0)
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<benchmark test=\"TestReadPerformance\" trials=\"500\" TestReadPerformance=\"1\" ", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        } // end of nop
        else if (strncmp (MyOpt, "benchmarkdigestperformance", MAX (strlen (MyOpt), strlen ("benchmarkdigestperformance") ) ) == 0)
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "<benchmark test=\"TestDigestPerformance\" trials=\"500\" TestDigestPerformance=\"1\" ", FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        } // end of nop

        else if (strncmp (MyOpt, "benchmarkddr", MAX (strlen (MyOpt), strlen ("benchmarkddr") ) ) == 0)
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer (tx_buffer, "<benchmark test=\"TestDDRValidity\" ", FIREHOSE_TX_BUFFER_SIZE);
            sprintf (temp_buffer, "trials=\"%lld\" ", MyTrials);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer (tx_buffer, "TestDDRValidity=\"1\" ", FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);

            // Save into XMLFileTable
            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            NumXMLFilesInTable++;

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }

            continue;
        } // end of nop


        else if (strncmp (MyOpt, "sendimage=", MAX (strlen (MyOpt), strlen ("sendimage=") ) ) == 0)
        {
            // Find image
            SIZE_T FileSizeNumSectors;

            char *SendImageFile = find_file (MyArg, 1);
            char * ffu_pch = NULL;
            struct FFUImage_t *FFU = NULL;

            if (SendImageFile == NULL)
            {
                // Couldn't find the file
                if (ConvertProgram2Read == 0)
                {
                    dbg (LOG_ERROR, "The file you specified with '--sendimage=%s' could not be found", MyArg);
                    ExitAndShowLog (1);
                }
                else
                {
                    // to be here it's a <read> so filesize is based on --num_sectors the user provided
                    FileSizeNumSectors = MyNumSectors;
                }
            }
            else
            {
                FileSizeNumSectors = LastFindFileFileSize / SectorSizeInBytes;

                if (LastFindFileFileSize % SectorSizeInBytes)
                {
                    FileSizeNumSectors++;
                }
            }


            dbg (LOG_DEBUG, "File size is %"SIZE_T_FORMAT" bytes", LastFindFileFileSize);
            dbg (LOG_DEBUG, "NumSectors needed %"SIZE_T_FORMAT, FileSizeNumSectors);

            if (MyNumSectors > 0)
            {
                // user specified some value
                if (MyNumSectors < FileSizeNumSectors)
                {
                    FileSizeNumSectors = MyNumSectors;
                    dbg (LOG_DEBUG, "NumSectors changed to %"SIZE_T_FORMAT" by --num_sectors=%"SIZE_T_FORMAT, FileSizeNumSectors, MyNumSectors);
                }
                else
                    dbg (LOG_WARN, "User specified --num_sectors=%"SIZE_T_FORMAT" but file only has %"SIZE_T_FORMAT" sectors. **Ignoring --num_sectors\n\n", MyNumSectors, FileSizeNumSectors);

            } // end if(MyNumSectors>0)

            if (FileSizeNumSectors == 0)
            {
                dbg (LOG_ERROR, "Nothing to send!! File is empty!! FileSizeNumSectors==0 for --sendimage=%s   USE --num_sectors=200 --sectorsizeinbytes=512", MyArg);
                ExitAndShowLog (1);
            }

            if (wipefirst)
            {
                InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
                AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
                AppendToBuffer (tx_buffer, "<program ", FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SectorSizeInBytes);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", ( (16 * 1024) / SectorSizeInBytes) );
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "filename=\"%s\" ", WipeFirstFileName);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", fh.attrs.physical_partition_number);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "start_sector=\"%s\" ", fh.attrs.start_sector);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);


                // Save into XMLFileTable
                CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
                NumXMLFilesInTable++;

                if (NumXMLFilesInTable >= MAX_XML_FILES)
                {
                    dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                    ExitAndShowLog (1);
                }

            } // end of if(wipefirst)

            if (erasefirst)
            {
                InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
                AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
                AppendToBuffer (tx_buffer, "<erase ", FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SectorSizeInBytes);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", FileSizeNumSectors );
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", fh.attrs.physical_partition_number);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "start_sector=\"%s\" ", fh.attrs.start_sector);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);


                // Save into XMLFileTable
                CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
                NumXMLFilesInTable++;

                if (NumXMLFilesInTable >= MAX_XML_FILES)
                {
                    dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                    ExitAndShowLog (1);
                }

            } // end of if(erasefirst)

            if (SendImageFile != NULL)
            {
                fd = ReturnFileHandle(SendImageFile, MAX_PATH_SIZE, "rb"); // will exit if not successful
            }
            ffu_pch = strstr(MyArg, ".ffu");

            if (SendImageFile != NULL && TestIfSparse(fd))
            {
                dbg(LOG_INFO, "%s is being treated as a sparse image", MyArg);
                GenerateSparseXMLTags(fd, MyArg, MyStartSector, SectorSizeInBytes, MyLun);
            }
            else if (SendImageFile != NULL && ffu_pch != NULL && TestIfWindowsFFU(fd))
            {
                dbg(LOG_INFO, "%s is being treated as a windows FFU image", MyArg);
                FFU = InitializeFFU(MyArg, SectorSizeInBytes, MyLun);
                ParseFFUHeaders(FFU, fd);
                ReadFFUGPT(FFU, fd);
                DumpFFURawProgram(FFU);
                CloseFFU(FFU);
            }
            else
            {
                InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
                AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);

                if (ConvertProgram2Read)
                {
                    AppendToBuffer (tx_buffer, "<read ", FIREHOSE_TX_BUFFER_SIZE);
                }
                else
                {
                    AppendToBuffer (tx_buffer, "<program ", FIREHOSE_TX_BUFFER_SIZE);

                    if (ConvertProgram2Firmware)
                        SectorSizeInBytes = 1;
                }

                sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SectorSizeInBytes);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", FileSizeNumSectors);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "filename=\"%s\" ", MyArg);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                //sprintf(temp_buffer,"physical_partition_number=\"%"SIZE_T_FORMAT"\" ",fh.attrs.physical_partition_number);
                sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", MyLun);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                //sprintf(temp_buffer,"start_sector=\"%s\" ",fh.attrs.start_sector);
                if (MyStartSector >= 0)
                  sprintf (temp_buffer, "start_sector=\"%"SIZE_T_FORMAT"\" Version=\"1\" ", MyStartSector);
                else
                  sprintf (temp_buffer, "start_sector=\"NUM_DISK_SECTORS%"SIZE_T_FORMAT"\" ", MyStartSector);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);


                // Save into XMLFileTable
                CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
                NumXMLFilesInTable++;
            }

            if (NumXMLFilesInTable >= MAX_XML_FILES)
            {
                dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                ExitAndShowLog (1);
            }


            continue;
        } // end of --sendimage=
        else if (strncmp (MyOpt, "stresstest", MAX (strlen (MyOpt), strlen ("stresstest") ) ) == 0)
        {
            FILE * fTemp;
            char MyBufferOfZeros[16 * 1024] = {0};
            long i;
            SIZE_T FileSizeNumSectors;

            dbg (LOG_DEBUG, "User wants stresstest - Also enabling verify_programming. Ensure this passes before leaving bring up lab\n");
            stresstest = 1;
            verify_programming = 1;

            dbg (LOG_INFO, "Creating 1GB all zero file '%s'\n", StressTestFileName);
            // Need to create the 16KB file of zeros
            fTemp = ReturnFileHandle (StressTestFileName, strlen (StressTestFileName), "wb"); // will exit if not successful

            for (i = 0; i < 65536; i++)
            {
                fwrite (MyBufferOfZeros, sizeof (MyBufferOfZeros), 1, fTemp);
                printf (".");
            }

            fclose (fTemp);
            fTemp = NULL;

            LastFindFileFileSize = 16 * 1024 * 65536;

            FileSizeNumSectors = LastFindFileFileSize / SectorSizeInBytes;

            if (LastFindFileFileSize % SectorSizeInBytes)
            {
                FileSizeNumSectors++;
            }


            dbg (LOG_DEBUG, "File size is %"SIZE_T_FORMAT" bytes", LastFindFileFileSize);
            dbg (LOG_DEBUG, "NumSectors needed %"SIZE_T_FORMAT, FileSizeNumSectors);


            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer (tx_buffer, "<program ", FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SectorSizeInBytes);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", FileSizeNumSectors);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "filename=\"%s\" ", StressTestFileName);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            //sprintf(temp_buffer,"physical_partition_number=\"%"SIZE_T_FORMAT"\" ",fh.attrs.physical_partition_number);
            sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", MyLun);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            if (MyStartSector >= 0)
              sprintf (temp_buffer, "start_sector=\"%"SIZE_T_FORMAT"\" ", MyStartSector);
            else
              sprintf (temp_buffer, "start_sector=\"NUM_DISK_SECTORS%"SIZE_T_FORMAT"\" ", MyStartSector);

            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);


            // Save into XMLFileTable
            for (i = 0; i < MyTrials; i++)
            {
                CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
                NumXMLFilesInTable++;

                if (NumXMLFilesInTable >= MAX_XML_FILES)
                {
                    dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                    ExitAndShowLog (1);
                }
            }

            continue;

        }

    }


    if (Simulate)
    {
        dbg (LOG_ALWAYS, "Remember: This *is* a simulation. Nothing will be sent to target, COM PORT NOT OPENED");
        dbg (LOG_ALWAYS, "      _                 _       _       ");
        dbg (LOG_ALWAYS, "     (_)               | |     | |      ");
        dbg (LOG_ALWAYS, "  ___ _ _ __ ___  _   _| | __ _| |_ ___ ");
        dbg (LOG_ALWAYS, " / __| | '_ ` _ \\| | | | |/ _` | __/ _ \\");
        dbg (LOG_ALWAYS, " \\__ \\ | | | | | | |_| | | (_| | ||  __/");
        dbg (LOG_ALWAYS, " |___/_|_| |_| |_|\\__,_|_|\\__,_|\\__\\___|\n");
    }
    else
    {
        if (NULL == port_name || strlen (port_name) == 0)
        {
            dbg (LOG_ERROR, "Port device name not specified; Example: --port=\\\\.\\COM19 ");
            ExitAndShowLog (1);
        }

        dbg (LOG_INFO, "User wants to talk to port '%s'", port_name);

        if (OpenPort (port_name) == 1)
        {
            ExitAndShowLog (1);
        }

        SetPortTimeouts();

        if (ReadBogusDataBeforeSendingConfigure)
        {
            // hack to get past weird sahara message in buffer
            dbg (LOG_ALWAYS, "**Attempting to read 4KB to flush out any possible strange first log packet since user chose ReadBogusDataBeforeSendingConfigure");
            BytesRead = ReadPort ( (unsigned char*) ReadBuffer, 4096, MAX_READ_BUFFER_SIZE); // null doesn't matter in RAW mode

            if (BytesRead > 0)
                PRETTYPRINT ( (unsigned char*) ReadBuffer, BytesRead, sizeof (ReadBuffer) );
        }
    }

    //GetNextPacket();  // this will set all variables, including GotACK

    if (createconfigxml)
    {
        if ( CopyString (ConfigXMLName, fh.attrs.MemoryName, strlen (ConfigXMLName), 0, strlen (fh.attrs.MemoryName), sizeof (fh.attrs.MemoryName), MAX_PATH_SIZE) == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into ConfigXMLName", fh.attrs.MemoryName, strlen (fh.attrs.MemoryName) );
            return 1;
        }

        if ( CopyString (ConfigXMLName, ".xml", strlen (ConfigXMLName), 0, strlen (".xml"), MAX_PATH_SIZE, 4) == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into ConfigXMLName", ".xml", strlen (".xml") );
            return 1;
        }

        fg = ReturnFileHandle (ConfigXMLName, MAX_PATH_SIZE, "w");  // will exit if not successful

        fprintf (fg, "<?xml version=\"1.0\" ?>\n");
        fprintf (fg, "<configuration>\n");
        fprintf (fg, "  <!--NOTE: This is an ** Autogenerated file **-->\n");

        szIndex = 0;

        for (i = 0; i < (SIZE_T) argc; i++) // skip 0 since argv[0] is fh_loader.exe
            szIndex += sprintf (&szTemp[szIndex], "%s ", argv[i]);

        szIndex = 0;

        fprintf (fg, "  <!--NOTE: %s -->\n", szTemp);

        fprintf (fg, "  <options>\n");
        fprintf (fg, "    DOWNLOAD_PROTOCOL       = FIREHOSE\n");

        if (LOAD_RAW_PROGRAM_FILES)
        {
            fprintf (fg, "    LOAD_RAW_PROGRAM_FILES  = true\n");
        }
        else
        {
            fprintf (fg, "    LOAD_RAW_PROGRAM_FILES  = false\n");
        }

        if (LOAD_PATCH_PROGRAM_FILES)
        {
            fprintf (fg, "    LOAD_PATCH_PROGRAM_FILES= true\n");
        }
        else
        {
            fprintf (fg, "    LOAD_PATCH_PROGRAM_FILES= false\n");
        }

        fprintf (fg, "  </options>\n");
        fprintf (fg, "  <search_paths>\n");

        fprintf (fg, "    %s\n", flattenbuildvariant);

        for (j = 0; j < num_search_paths; j++)
            fprintf (fg, "    %s\n", search_path[j]);

        fprintf (fg, "  </search_paths>\n");
        fprintf (fg, "  <rawprogram>\n");

        for (i = 0; i < NumContensXML; i++)
        {
            if (ContensXML[i].FileType == 'r')
            {
                //if(fh.attrs.MemoryName[0] == ContensXML[i].StorageType) // || ContensXML[i].StorageType==0 )
                if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0 )
                    fprintf (fg, "    %s\n", ContensXML[i].Filename);
            }
        }

        fprintf (fg, "  </rawprogram>\n");
        fprintf (fg, "  <patch>\n");

        for (i = 0; i < NumContensXML; i++)
        {
            if (ContensXML[i].FileType == 'p')
            {
                //if(fh.attrs.MemoryName[0] == ContensXML[i].StorageType) // || ContensXML[i].StorageType==0 )
                if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0 )
                    fprintf (fg, "    %s\n", ContensXML[i].Filename);
            }
        }

        fprintf (fg, "  </patch>\n");

        fprintf (fg, "  <file_mappings>\n");
        fprintf (fg, "  NOTE: Do not trust these mappings. These are only POSSIBLE file/path mappings\n");
        fprintf (fg, "  NOTE: If any of the files are *also* found in the search paths listed above, that file will be used\n\n");

        for (i = 0; i < NumContensXML; i++)
        {
            if (ContensXML[i].FileType != 'r' && ContensXML[i].FileType != 'p')
            {
                if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0 )
                {
                    fprintf (fg, "    %s", ContensXML[i].Path);
                    fprintf (fg, "%s\n", ContensXML[i].Filename);
                }
            }
        }

        fprintf (fg, "  </file_mappings>\n");
        fprintf (fg, "  </configuration>\n");

        fclose (fg);
        fg = NULL;
    }

    if (createcommandtrace)
    {
        fc = ReturnFileHandle (CommandTraceName, MAX_PATH_SIZE, "w");   // will exit if not successful
        fclose (fc);
        fc = NULL;
    }


    MaxNumDigestsPerTable = fh.attrs.MaxDigestTableSizeInBytes / DigestSizeInBytes;
    CurrentDigestLocation   = MaxNumDigestsPerTable - 54; // default to this. SendSignedDigestTable() below will correct this

    if (CreateDigests)
    {
        ft = ReturnFileHandle ("DIGEST_TABLE.bin", strlen ("DIGEST_TABLE.bin"), "wb"); // will exit if not successful
        fclose (ft);
        ft = NULL;
    }


//  BytesWritten = WritePort(buffer, strlen((const char *)buffer) );
//  PRETTYPRINT(buffer, BytesWritten);
    //handleNop();  // turns on VERBOSE, if you don't want that, send 2

    LoadConfigureIntoStringTable(); // always loads at location 0 (i.e. NumXMLFilesInTable=0)

    if (AllowReset)
        LoadResetIntoStringTable(); // always loads at NumXMLFilesInTable (i.e. the last location)

    //SendConfigure();
    //handleNop();  // turns on VERBOSE, if you don't want that, send 2

    ModifyTags(); // this optionally converts <program to <read if user specified --convertprogram2read

    // SNAPSHOT TIME HERE
    gettimeofday (&fh_loader_start, NULL);

    if (SortTags)
    {
        // ensure that <erase> is first and <patch> tags are last
        dbg (LOG_INFO, "Sorting TAGS to ensure order is <configure>,<erase>, others, <patch>,<power>");
        dbg (LOG_INFO, "If  you don't want this, use --dontsorttags\n");
        SortMyXmlFiles();
    }
    else
        dbg (LOG_INFO, "*Not* sorting tags since --dontsorttags was provided\n");


    if (showpercentagecomplete)
    {
        // For Corporate Tools they want to show "52% loaded" type of message. I don't know ahead of time how big the build is
        // This is made especially tricky since --files or --notfiles, therefore the best way is to do a simulation
        SimulateBack    = Simulate; // backup
        Simulate      = 1;
        SimulateForFileSize = 1;  // this makes simulation quit right after getting file size
        BuildSizeTransferSoFar = 0;
        SendSignedDigestTable (SignedDigestTableFilename);  // just returns if SignedDigestTableFilename is empty
        SendXmlFiles();       // this will modify BuildSizeTransferSoFar

        TotalTransferSize = BuildSizeTransferSoFar;

        //If we are verifying the build it will the same ammount of data again. It isn't perfect (because of sector rounding)
        // more accurate than not doing it.
        if (verify_programming)
        {
            TotalTransferSize *= 2.00;
        }

        if (TotalTransferSize > 0)
        {
            ReturnSizeString (TotalTransferSize, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
            dbg (LOG_INFO, "\n\nTotal to be tansferd with <program> or <read> is %s\n\n", temp_buffer);
        }

        Simulate      = SimulateBack; // restore
        SimulateForFileSize = 0;      // no longer just for file size
    }

    BuildSizeTransferSoFar = 0;
    SendSignedDigestTable (SignedDigestTableFilename);  // just returns if SignedDigestTableFilename is empty
    SendXmlFiles();

    // SNAPSHOT TIME HERE
    gettimeofday (&fh_loader_end, NULL);


    if (!Simulate)
        ClosePort();


    if (CreateDigests)
    {
        PrepareDigestTable();
    }


    dbg (LOG_ALWAYS, "==============================================================");
    dbg (LOG_ALWAYS, "Files used and their paths");

    for (i = 0; i < FileToTrackCount; i++)
    {
        dbg (LOG_ALWAYS, "%3"SIZE_T_FORMAT" '%s'", i + 1, MaxFilesToTrack[i]);
    }

    printf ("\n");

    if (strlen (WarningsBuffer) > 0)
    {
        display_warning (NULL);
        dbg (LOG_ALWAYS, "==============================================================");
        dbg (LOG_ALWAYS, "NOTE: There were WARNINGS!! Repeated here, but please see log for more detail");
        //dbg(LOG_ALWAYS,"%s",WarningsBuffer);  <-- this wants \\snowcone\blah submitted as \\\\snowcone\\blah, debug this later
        printf ("%s\nNOTE: There were WARNINGS!! Repeated above, but please see log for more detail\n\n\n", WarningsBuffer);
        dbg (LOG_ALWAYS, "==============================================================");
    }

    if (Simulate)
    {
        dbg (LOG_ALWAYS, "Remember: This was a simulation. Nothing was sent to a target");
        dbg (LOG_ALWAYS, "      _                 _       _       ");
        dbg (LOG_ALWAYS, "     (_)               | |     | |      ");
        dbg (LOG_ALWAYS, "  ___ _ _ __ ___  _   _| | __ _| |_ ___ ");
        dbg (LOG_ALWAYS, " / __| | '_ ` _ \\| | | | |/ _` | __/ _ \\");
        dbg (LOG_ALWAYS, " \\__ \\ | | | | | | |_| | | (_| | ||  __/");
        dbg (LOG_ALWAYS, " |___/_|_| |_| |_|\\__,_|_|\\__,_|\\__\\___|\n");
    }


    dbg (LOG_ALWAYS, "     _             (done)");
    dbg (LOG_ALWAYS, "    | |                 ");
    dbg (LOG_ALWAYS, "  __| | ___  _ __   ___ ");
    dbg (LOG_ALWAYS, " / _` |/ _ \\| '_ \\ / _ \\");
    dbg (LOG_ALWAYS, "| (_| | (_) | | | |  __/");
    dbg (LOG_ALWAYS, " \\__,_|\\___/|_| |_|\\___|");

    if (ThereWereErrors)
        dbg (LOG_ALWAYS, "{Not Finished Successfully - There were ERRORS}\n");
    else
        dbg (LOG_ALWAYS, "{All Finished Successfully}\n");


    //dbg(LOG_ALWAYS,"Total time was %6.2f seconds\n",ReturnTimeInSeconds(&fh_loader_start,&fh_loader_end));

    time_throughput_calculate (&fh_loader_start, &fh_loader_end, BuildSizeTransferSoFar, 0.0, LOG_INFO );


    if (createconfigxml)
        dbg (LOG_ALWAYS, "\n\n\nCreated '%s'.\n\n"
             "NOTE: You don't need to use --contentsxml= anymore. YOU CAN NOW USE --sendxml=%s\n\n", ConfigXMLName, ConfigXMLName);

    if (verify_build)
    {
        if (DeviceProgrammerIndex >= 0)
        {
            dbg (LOG_INFO, "\n\n");
            dbg (LOG_INFO, "QSaharaServer.exe -p \\\\.\\COM27 -s 13:%s -b %s\n", ContensXML[DeviceProgrammerIndex].Filename, ContensXML[DeviceProgrammerIndex].Path);
        }
    }

    PromptUser = 0; // turn this off here
    if(ThereWereErrors)
      ExitAndShowLog (1); // this will prompt user to press any key if needed
    else
      ExitAndShowLog (0); // this will prompt user to press any key if needed

} // end main()

void PrepareDigestTable (void)
{
    SIZE_T NumDigestsInFile, Remainder, NumTablesAfter1st, DigestsPerTable, DigestsInLastTable;
    SIZE_T i, SeekLoc, HashOfNextTableLoc, TempValue;
    FILE *FTemp;
    unsigned long long DigestFileSize;

    char *DigestFile = NULL;

    DigestFile = find_file ("DIGEST_TABLE.bin", 1);

    if (DigestFile == NULL)
    {
        dbg (LOG_ERROR, "'%s' not found", "DIGEST_TABLE.bin");
        ExitAndShowLog (1);
    }

    BytesRead = 0;

    NumDigestsInFile = LastFindFileFileSize / 32;
    Remainder = LastFindFileFileSize % 32;

    if (Remainder > 0)
    {
        dbg (LOG_ERROR, "Your DIGEST TABLE '%s' file is not a multiple of 32 bytes",  "DIGEST_TABLE.bin");
        ExitAndShowLog (1);
    }

    dbg (LOG_ALWAYS, "NumDigestsInFile = %"SIZE_T_FORMAT, NumDigestsInFile);


    if (MaxNumDigestsPerTable == 0)
    {
        dbg (LOG_INFO, "3. In PrepareDigestTable AND MaxNumDigestsPerTable WAS ZERO");
        MaxNumDigestsPerTable = fh.attrs.MaxDigestTableSizeInBytes / DigestSizeInBytes;
    }

    DigestsPerTable = MaxNumDigestsPerTable - 1; // -1 to make room for hash of next table

    if (NumDigestsInFile >= 54)
    {
        NumTablesAfter1st = NumDigestsInFile - 53;
        NumTablesAfter1st = ( (NumDigestsInFile - 53) / DigestsPerTable) + 1;
        DigestsInLastTable = (NumDigestsInFile - 53) % DigestsPerTable;
    }
    else
    {
        NumTablesAfter1st = 0;
        DigestsInLastTable = 0;
    }

    HashOfNextTableLoc = NumTablesAfter1st - 1;

    ft = ReturnFileHandle ("DIGEST_TABLE.bin", strlen ("DIGEST_TABLE.bin"), "rb"); // will exit if not successful


    memset (tx_buffer,        0x0, FIREHOSE_TX_BUFFER_SIZE); // this is strictly not needed
    memset (tx_buffer_backup, 0x0, FIREHOSE_TX_BUFFER_SIZE);
    memset (temp_hash_value,  0x0, 32);

    // Pass 1: Create HASHES of the next tables and store them in tx_buffer_backup
    for (i = 0; i < NumTablesAfter1st; i++)
    {
        // seek to location

        SeekLoc = (53 + HashOfNextTableLoc * DigestsPerTable) * DigestSizeInBytes;

        fseek (ft, SeekLoc, SEEK_SET);

        BytesRead = fread (&tx_buffer, sizeof (char), fh.attrs.MaxDigestTableSizeInBytes - 32, ft);

        // this copies last hash into current table in order to calculate the hash
        memscpy (&tx_buffer[fh.attrs.MaxDigestTableSizeInBytes - 32], FIREHOSE_TX_BUFFER_SIZE, temp_hash_value, sizeof (temp_hash_value) ); //

        if (i > 0) // On the very last table, if there is only 1 hash left, then above copied an all zero hash. Don't consider this as additional data
            BytesRead += 32;

        DigestFileSize = ftell (ft);
        fseek (ft, 0, SEEK_END);

        // USB will hang if we send any multiple of 512. Add 1 byte to the last table to get around it.
        if (DigestFileSize == ftell (ft) )
        {
            tx_buffer[BytesRead] = 0;
            BytesRead += 1;
        }

        sechsharm_sha256_init  (&context_per_packet);
        sechsharm_sha256_update (&context_per_packet, context_per_packet.leftover, & (context_per_packet.leftover_size), (unsigned char *) tx_buffer, BytesRead);
        sechsharm_sha256_final (&context_per_packet, context_per_packet.leftover, & (context_per_packet.leftover_size), temp_hash_value);

        memscpy (&tx_buffer_backup[HashOfNextTableLoc * DigestSizeInBytes], FIREHOSE_TX_BUFFER_SIZE, temp_hash_value, sizeof (temp_hash_value) ); // memcpy
        HashOfNextTableLoc--;

    }


    // Now create DigestsToSign.bin
    FTemp = ReturnFileHandle ("DigestsToSign.bin.mbn", strlen ("DigestsToSign.bin.mbn"), "wb"); // will exit if not successful
    fseek (ft, 0, SEEK_SET); // back to the beginning of the file
    BytesRead = fread (&tx_buffer, sizeof (char), 53 * DigestSizeInBytes, ft);

    TempValue = 0x1A;
    fwrite (&TempValue, 4, 1, FTemp); // SW_ID
    TempValue = 0x03;
    fwrite (&TempValue, 4, 1, FTemp); // header_vsn_num Header version number
    TempValue = 40;
    fwrite (&TempValue, 4, 1, FTemp); // image_src Location of image in flash
    TempValue = 0x00;
    fwrite (&TempValue, 4, 1, FTemp); // image_dest_ptr

    if (NumDigestsInFile >= 54)
        TempValue = 54 * 32;
    else
    {
        // to be here is 53 or less, so I will add an all zero entry at the end
        /*
              if(NumDigestsInFile>=53)
                TempValue = NumDigestsInFile*32;  // if exactly 53, do nothing
              else
                TempValue = (NumDigestsInFile+1)*32;  // adding a padding of all zero hash at the end, so that DeviceProgrammer will
                                    // not load to the end of the table, thus expecting another table
        */
        TempValue = (NumDigestsInFile + 1) * 32; // adding a padding of all zero hash at the end, so that DeviceProgrammer will
        // not load to the end of the table, thus expecting another table

    }

    fwrite (&TempValue, 4, 1, FTemp); // Size of complete image in bytes
    fwrite (&TempValue, 4, 1, FTemp); // Size of code region of image in bytes

    TempValue = 0x00;
    fwrite (&TempValue, 4, 1, FTemp); // Pointer to images attestation signature
    TempValue = 0x00;
    fwrite (&TempValue, 4, 1, FTemp); // Size of the attestation signature in bytes
    TempValue = 0x00;
    fwrite (&TempValue, 4, 1, FTemp); // Pointer to the chain of attestation certificates associated with the image
    TempValue = 0x00;
    fwrite (&TempValue, 4, 1, FTemp); // Size of the attestation chain in bytes

    fwrite (tx_buffer, BytesRead, 1, FTemp);

    // Now write hash of the next table
    if (NumDigestsInFile < 54)
        memset (tx_buffer_backup, 0, sizeof (tx_buffer_backup) );

    fwrite (&tx_buffer_backup[0], 32, 1, FTemp);

    fclose (FTemp);
    FTemp = NULL;


    // Pass 2: Create ChainedTableOfDigests.bin
    FTemp = ReturnFileHandle ("ChainedTableOfDigests.bin", strlen ("ChainedTableOfDigests.bin"), "wb"); // will exit if not successful

    for (i = 0; i < NumTablesAfter1st; i++)
    {
        // seek to location
        SeekLoc = (53 + i * DigestsPerTable) * DigestSizeInBytes;
        fseek (ft, SeekLoc, SEEK_SET);

        memset (tx_buffer,        0x0, FIREHOSE_TX_BUFFER_SIZE); // this is strictly not needed

        BytesRead = fread (&tx_buffer, sizeof (char), fh.attrs.MaxDigestTableSizeInBytes - 32, ft);

        fwrite (tx_buffer, BytesRead, 1, FTemp);

        if (i < NumTablesAfter1st - 1)
        {
            // Now write hash of the next table
            fwrite (&tx_buffer_backup[ (i + 1) * 32], 32, 1, FTemp);
        }
        else
        {
            //On the last table add one byte to make sure it is a short packet
            // this gets around the requirement that the last packet can't a multiple of 512.
            tx_buffer[0] = 0;
            fwrite (tx_buffer, 1, 1, FTemp);
        }


    }

    DigestFileSize = ftell (FTemp);

    if (DigestFileSize % 512 == 0 && DigestFileSize % fh.attrs.MaxDigestTableSizeInBytes != 0)
    {
        dbg (LOG_ERROR, "This should not be hit. The Digest output file is a multiple of 512. This will cause the target to hang. Please re-run and add --nop to fix this issue");
        ExitAndShowLog (1);
    }

    fclose (FTemp);
    FTemp = NULL;

    fclose (ft);
    ft = NULL;
}

void TestIfWeNeedToSendDigestTable (void)
{
    char *TempFile = NULL, BackRawMode;
    FILE *fd;
    SIZE_T BytesRead, FileSize;

    if (!UsingValidation)
    {
        return;  // not even using VIP, so exit
    }

    //if(MaxNumDigestsPerTable>NumDigestsFound)
    if (NumDigestsFound <= 53)
    {
        //dbg(LOG_INFO,"First table only had %"SIZE_T_FORMAT" digests, so no need to send chained digests",NumDigestsFound);
        return;
    }

    if ( (CurrentDigestLocation + 1) % MaxNumDigestsPerTable == 0)
    {
        // Ex. every 224 packets we send the digest table, as in 0, 223, 447, etc
    }
    else
    {
        // don't need to send a Digest Table yet
        return;
    }

    // to be here means we need to send a Digest Table

    // Backup tx_buffer into tx_buffer_backup first
    memscpy (tx_buffer_backup, FIREHOSE_TX_BUFFER_SIZE, tx_buffer, FIREHOSE_TX_BUFFER_SIZE); // memcpy

    if (strlen (ChainedDigestTableFilename) == 0)
    {
        //return;
        dbg (LOG_ERROR, "ChainedDigestTableFilename was not set with --chaineddigests=SomeFilename.bin and you have at least %"SIZE_T_FORMAT" digests", NumDigestsFound);
        ExitAndShowLog (1);
    }


    dbg (LOG_DEBUG, "\t _               _       _        _     _      ");
    dbg (LOG_DEBUG, "\t| |             | |     | |      | |   | |     ");
    dbg (LOG_DEBUG, "\t| |__   __ _ ___| |__   | |_ __ _| |__ | | ___ ");
    dbg (LOG_DEBUG, "\t| '_ \\ / _` / __| '_ \\  | __/ _` | '_ \\| |/ _ \\");
    dbg (LOG_DEBUG, "\t| | | | (_| \\__ \\ | | | | || (_| | |_) | |  __/");
    dbg (LOG_DEBUG, "\t|_| |_|\\__,_|___/_| |_|  \\__\\__,_|_.__/|_|\\___|");

    dbg (LOG_DEBUG, "Since CurrentDigestLocation=%"SIZE_T_FORMAT", Need to send digest table here digest_file_offset=%"SIZE_T_FORMAT, CurrentDigestLocation, digest_file_offset);

    // if digestTableSize is 992 bytes, this is 31 digests per table. Meaning we send Singed DigestTable (1st command) then next 30 commands for a total of 31
    // Ex   Command -1 is Digest Table, commands 0  to 29  <-- i.e. 30 commands + Digest Table is 31 commands
    // then Command 30 is Digest Table, commands 31 to 60  <-- i.e. 30 commands + Digest Table is 31 commands
    // then Command 61 is Digest Table, commands 62 to 91  <-- i.e. 30 commands + Digest Table is 31 commands
    // then Command 92 is Digest Table, commands 93 to 122 <-- i.e. 30 commands + Digest Table is 31 commands

    // find this file ChainedDigestTableFilename
    TempFile = find_file (ChainedDigestTableFilename, 0);

    if (TempFile == NULL)
    {
        dbg (LOG_ERROR, "'%s' not found", ChainedDigestTableFilename);
        ExitAndShowLog (1);
    }

    fd = ReturnFileHandle (TempFile, MAX_PATH_SIZE, "rb"); // will exit if not successful

    FileSize = ReturnFileSize (fd);
    dbg (LOG_DEBUG, "File size is %"SIZE_T_FORMAT" bytes", FileSize);

    if (FileSize == 0)
    {
        dbg (LOG_INFO, "--chaineddigests file was 0 bytes, not sending any chained digest tables");
        return;
    }


    if (digest_file_offset > 0)
        fseek (fd, digest_file_offset, SEEK_SET);

    dbg (LOG_DEBUG, "At location %"SIZE_T_FORMAT" in '%s' attempting to read %"SIZE_T_FORMAT" bytes", digest_file_offset, ChainedDigestTableFilename, fh.attrs.MaxDigestTableSizeInBytes);

    BytesRead = fread (&tx_buffer, sizeof (char), fh.attrs.MaxDigestTableSizeInBytes, fd);
    fclose (fd);
    fd = NULL;

    dbg (LOG_DEBUG, "BytesRead is %"SIZE_T_FORMAT" bytes", BytesRead);

    if (BytesRead == 0)
    {
        dbg (LOG_ERROR, "Read 0 bytes from '%s'", ChainedDigestTableFilename);
        ExitAndShowLog (1);
    }

    // ------------------// ------------------// ------------------// ------------------
    // ------------------// ------------------// ------------------// ------------------
    UsingValidation = 0;  // NOTE - TURNING OFF VIP HERE WHEN SENDING THIS BUFFER to avoid recursion into TestIfWeNeedToSendDigestTable()
    UsingValidation = 0;  // NOTE - TURNING OFF VIP HERE WHEN SENDING THIS BUFFER to avoid recursion into TestIfWeNeedToSendDigestTable()

    if ( sendTransmitBufferBytes ( BytesRead ) == 0)
    {
        ExitAndShowLog (1);
    }

    UsingValidation = 1;  // NOTE - TURNING VIP *BACK ON* HERE
    UsingValidation = 1;  // NOTE - TURNING VIP *BACK ON* HERE
    // ------------------// ------------------// ------------------// ------------------
    // ------------------// ------------------// ------------------// ------------------



    // Put tx_buffer_backup into tx_buffer first
    memscpy (tx_buffer, FIREHOSE_TX_BUFFER_SIZE, tx_buffer_backup, FIREHOSE_TX_BUFFER_SIZE); // memcpy



    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    BackRawMode = RawMode;  // Backup this flag
    RawMode = 0;      // turn off RawMode to ensure XML parsing happens on incoming packet
    GetNextPacket();    // Get the packet
    RawMode = BackRawMode;  // Put back whatever state we were in

    //printf("\n\n\n\n\n\nAARON - Reading twice");
    //GetNextPacket();  // this will set all variables, including GotACK

    if (GotACK)
    {
        dbg (LOG_DEBUG, "Got the ACK");
        dbg (LOG_DEBUG, "DIGEST TABLE ACCEPTED - Just RESET CurrentDigestLocation to 0, since now using a new table");
        CurrentDigestLocation = 0;
        digest_file_offset += MaxNumDigestsPerTable * DigestSizeInBytes;
    }
    else
    {
        dbg (LOG_ERROR, "Digest Table was NOT ACCEPTED. Can't continue");
        ExitAndShowLog (1);
    }
}

void SendSignedDigestTable (char *SignedDigestTableFilename)
{
    char *TempFile = NULL;
    FILE *fd;
    SIZE_T BytesRead, FileSize;

    if (strlen (SignedDigestTableFilename) == 0)
        return;

    UsingValidation   = 1;
    ShowDigestPerPacket = 1;

    // find this file SignedDigestTableFilename
    TempFile = find_file (SignedDigestTableFilename, 1);

    if (TempFile == NULL)
    {
        dbg (LOG_ERROR, "'%s' not found", SignedDigestTableFilename);
        ExitAndShowLog (1);
    }

    dbg (LOG_INFO, "Signed Digest Table (%"SIZE_T_FORMAT" bytes) '%s' ", LastFindFileFileSize, full_filename_with_path);

    fd = ReturnFileHandle (TempFile, MAX_PATH_SIZE, "rb"); // will exit if not successful

    FileSize = ReturnFileSize (fd);

    dbg (LOG_DEBUG, "File size is %"SIZE_T_FORMAT" bytes", (FileSize) );

    BytesRead = fread (&tx_buffer, sizeof (char), FileSize, fd);
    fclose (fd);
    fd = NULL;

    if (BytesRead == 0)
    {
        dbg (LOG_ERROR, "Read 0 bytes from '%s'", SignedDigestTableFilename);
        ExitAndShowLog (1);
    }

    if (BytesRead < FileSize)
    {
        dbg (LOG_ERROR, "Read %"SIZE_T_FORMAT" bytes from '%s' but expected %"SIZE_T_FORMAT, BytesRead, SignedDigestTableFilename, FileSize);
        ExitAndShowLog (1);
    }


    if (FileSize > (40 + 256 + (6 * 1024) ) )
    {
        NumDigestsFound             = (FileSize - 40 - 256 - (6 * 1024) ) / DigestSizeInBytes;
    }
    else
    {
        // probably not signed and this is for testing
        // I'm assuming it at least has the 40 byte header. Check if NumDigestsFound is negative
        NumDigestsFound             = (FileSize - 40) / DigestSizeInBytes;
    }

    //if(NumDigestsFound < 0:
    //log_error("Something is wrong with your digest table. For testing it at least needs a 40 byte MBN header")
    //log_error("Ideally it should be fully digitally signed")
    //ExitAndShowLogName()

    dbg (LOG_INFO, "SIGNED Digest table had %"SIZE_T_FORMAT" digests", NumDigestsFound);

    dbg (LOG_INFO, "MaxNumDigestsPerTable is %"SIZE_T_FORMAT, MaxNumDigestsPerTable);

    CurrentDigestLocation = MaxNumDigestsPerTable - NumDigestsFound;
    CurrentDigestLocation   = MaxNumDigestsPerTable - 54;
    dbg (LOG_INFO, "Setting CurrentDigestLocation to number %"SIZE_T_FORMAT", meaning %"SIZE_T_FORMAT" hashes to go before a new table must be sent", CurrentDigestLocation, MaxNumDigestsPerTable - CurrentDigestLocation);


    // ------------------// ------------------// ------------------// ------------------
    // ------------------// ------------------// ------------------// ------------------
    UsingValidation = 0;  // NOTE - TURNING OFF VIP HERE WHEN SENDING THIS BUFFER
    UsingValidation = 0;  // NOTE - TURNING OFF VIP HERE WHEN SENDING THIS BUFFER

    dbg (LOG_INFO, "Calling sendTransmitBufferBytes(%"SIZE_T_FORMAT" bytes)", FileSize);

    if ( sendTransmitBufferBytes ( FileSize ) == 0)
    {
        ExitAndShowLog (1);
    }

    UsingValidation = 1;  // NOTE - TURNING VIP *BACK ON* HERE
    UsingValidation = 1;  // NOTE - TURNING VIP *BACK ON* HERE
    // ------------------// ------------------// ------------------// ------------------
    // ------------------// ------------------// ------------------// ------------------




    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if (GotACK)
        dbg (LOG_DEBUG, "Got the ACK");
    else
    {
        dbg (LOG_ERROR, "The Digitally Signed Digest Table was rejected by the target\n"
             "\n\t1. Are you sure it was digitally signed? Was it digitally signed correctly for this target?"
             "\n\t2. Are you sure you have a secure boot enabled target? Maybe your target was expecting an XML file?"
             "\n\nBecause you used --signeddigests=%s option, this tool sends "
             "\n'%s' as the first packet. It is sent as a RAW binary file for VIP"
             "\nYour target either wasn't expecting this file, OR your file was *not* digitially signed correctly"
             "\n\nCan't continue due to this error. Your target does *not* like what you sent\n\n", SignedDigestTableFilename, SignedDigestTableFilename);
        ExitAndShowLog (1);
    }

    dbg (LOG_INFO, "\t _____ _                   _                  ");
    dbg (LOG_INFO, "\t/  ___(_)                 | |                 ");
    dbg (LOG_INFO, "\t\\ `--. _  __ _ _ __   __ _| |_ _   _ _ __ ___ ");
    dbg (LOG_INFO, "\t `--. \\ |/ _` | '_ \\ / _` | __| | | | '__/ _ \\");
    dbg (LOG_INFO, "\t/\\__/ / | (_| | | | | (_| | |_| |_| | | |  __/");
    dbg (LOG_INFO, "\t\\____/|_|\\__, |_| |_|\\__,_|\\__|\\__,_|_|  \\___|");
    dbg (LOG_INFO, "\t          __/ |                               ");
    dbg (LOG_INFO, "\t         |___/   \n");
    dbg (LOG_INFO, "\t______                      _ ");
    dbg (LOG_INFO, "\t| ___ \\                    | |");
    dbg (LOG_INFO, "\t| |_/ /_ _ ___ ___  ___  __| |");
    dbg (LOG_INFO, "\t|  __/ _` / __/ __|/ _ \\/ _` |");
    dbg (LOG_INFO, "\t| | | (_| \\__ \\__ \\  __/ (_| |");
    dbg (LOG_INFO, "\t\\_|  \\__,_|___/___/\\___|\\__,_|\n");
    dbg (LOG_DEBUG, "FIREHOSE: Signed DIGEST sent. This is not counted towards command sent");
    dbg (LOG_DEBUG, "FIREHOSE: PacketsSent reset to 0");

    PacketsSent = 0;
    CurrentDigestLocation -= 1;  // Go back 1 since DigSig table doesn't count
}

void SetPortTimeouts (void)
{
    // TIMEOUTS

#ifdef _MSC_VER // i.e. if compiling under Windows
    timeouts.ReadIntervalTimeout        = MAXDWORD; // maximum number of milliseconds that can elapse between two characters without a timeout occurring
    timeouts.ReadTotalTimeoutMultiplier = 0;    // For each read operation, this number is multiplied by the number of bytes that the read operation expects to receive.
    timeouts.ReadTotalTimeoutConstant   = 100;    // actual timeout is this in ms
    timeouts.WriteTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant  = 10;   // 10 seems to take 1 minute when HW is in a messed up state

    if (!SetCommTimeouts (port_fd, &timeouts) ) // Error setting time-outs
    {
        dbg (LOG_ERROR, "Error setting com port timeouts");
        ExitAndShowLog (1);
    }

#else
    memset (&tio, 0, sizeof (tio) );
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CREAD | CLOCAL;     // 8n1, see termios.h for more information
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 5;
    cfsetospeed (&tio, B115200);          // 115200 baud
    cfsetispeed (&tio, B115200);          // 115200 baud
    tcsetattr (port_fd, TCSANOW, &tio);
#endif
}

void StripNewLines (char *Buffer, SIZE_T BufferLength, SIZE_T Start, SIZE_T Length)
{
    SIZE_T i;

    if (Start + Length > BufferLength)
        return;

    for (i = 0; i < Length; i++)
    {
        if (Buffer[Start + i] == '\n' || Buffer[Start + i] == '\r')
            Buffer[Start + i] = 0x20; // make a space
    }

}

void PrettyPrintHexValueIntoTempBuffer (uint8 *temp_hash_val, int length, int offset, int MaxLength)
{
    int i;

    if (offset + (2 * length) > FIREHOSE_TX_BUFFER_SIZE)
        return; // this is too big

    if (length > MaxLength)
        return; // this is too big

    for (i = 0; i < length; i++)
    {
        sprintf (&temp_buffer[offset + i * 2], "%02X", temp_hash_val[i]);
    }

    temp_buffer[offset + i * 2] = '\0';
}

unsigned int WritePort (unsigned char *pData, unsigned int length, unsigned int MaxLength, unsigned char RawData)
{
    unsigned int BytesWritten = 0, i = 0, NumTries = 100;
    static int HashTablesSent = 1;  // start at 1 since the signed digest table was #0
    enum LOG_TYPES LogType = LOG_DEBUG;
    char answer = 'y';

    if (Interactive)
        LogType = LOG_INFO;

    if (length > MaxLength)
    {
        dbg (LOG_ERROR, "In WritePort and length %d bytes is greater than buffer size of %d bytes", length, MaxLength);
        ExitAndShowLog (1);
    }

    if (ShowDigestPerPacket)
    {
        sechsharm_sha256_init  (&context_per_packet);
        sechsharm_sha256_update (&context_per_packet, context_per_packet.leftover, & (context_per_packet.leftover_size), (unsigned char *) pData, length);
        sechsharm_sha256_final (&context_per_packet, context_per_packet.leftover, & (context_per_packet.leftover_size), temp_hash_value);

        // Show some of the rawdata packet COMMAND_TRACE_BYTES_TO_RECORD/2 since each HEX value takes 2 bytes
        PrettyPrintHexValueIntoTempBuffer (temp_hash_value, 32, 0, 32); // from, size, offset, maxlength
    }

    if (RawMode)
    {
        char Size1[2048];

        ReturnSizeString (length, Size1, sizeof (Size1) );

        if (!ShowDigestPerPacket)
            dbg (LogType, "CHANNEL DATA (P%"SIZE_T_FORMAT4") (H%"SIZE_T_FORMAT5") (%s) - HOST TO TARGET -->", PacketsSent, CurrentDigestLocation, Size1);
        else
            dbg (LogType, "CHANNEL DATA (P%"SIZE_T_FORMAT4") (H%"SIZE_T_FORMAT5") (%s) - HOST TO TARGET --> SHA256 DIGEST: %s", PacketsSent, CurrentDigestLocation, Size1, temp_buffer);
    }
    else
    {
        if (!ShowDigestPerPacket)
            dbg (LogType, "CHANNEL DATA (P%"SIZE_T_FORMAT4") (H%"SIZE_T_FORMAT5") (%7d bytes) - HOST TO TARGET -->\n"
                 "===========================================================================================================\n"
                 "%s\n"
                 "============================================================================================================\n"
                 , PacketsSent, CurrentDigestLocation, length, pData);
        else
            dbg (LogType, "CHANNEL DATA (P%"SIZE_T_FORMAT4") (H%"SIZE_T_FORMAT5") (%7d bytes) - HOST TO TARGET --> SHA256 DIGEST: %s\n"
                 "===========================================================================================================\n"
                 "%s\n"
                 "============================================================================================================\n"
                 , PacketsSent, CurrentDigestLocation, length, temp_buffer, pData);
    }

    //dbg(LOG_DEBUG, "In WritePort(unsigned char *pData, %d)\n",length);

    if (PrettyPrintRawPacketsToo == 1)
        PRETTYPRINT (pData, length, MaxLength); // always show everything
    else if (RawData == 0 && VerboseLevel >= LOG_DEBUG)
        PRETTYPRINT (pData, length, MaxLength); // else, only if it is an XML file and user didn't request silent


    if ( (Interactive == 2 && RawData == 1) || (Interactive == 1 && RawData == 0) )
    {
        printf ("\n*INTERACTIVE* Press any key to **SEND** the above %d bytes (q to quit interactive): ", length);
        answer = getchar();
        fseek (stdin, 0, SEEK_END); // get rid of extra \n keys

        if (answer == 'q')
            Interactive = 0;

        printf ("\n\n");
    }


    if (Simulate)
        BytesWritten = length; //strlen((const char *)tx_buffer);
    else
    {

        for (i = 0; i < NumTries; i++)
        {

#ifdef _MSC_VER // i.e. if compiling under Windows

            if (!WriteFile (port_fd, pData, length, (LPDWORD) &BytesWritten , NULL) )
            {
                // to be here means more data needs to be written still
                pData += BytesWritten;
            }

#else
            dbg (LOG_INFO, "Trying to write %d bytes", length);
            BytesWritten = write (port_fd, pData, length);

            if (BytesWritten < 0)
            {
                dbg (LOG_ERROR, "BytesWritten was < 0 - Could not write to '%s', ret=0x%X\n", pData, BytesWritten);
                //return 1;
            }
            else
            {
                // to be here means more data needs to be written still
                pData += BytesWritten;
            }

            dbg (LOG_INFO, "Just wrote %d bytes", BytesWritten);

#endif

            if (BytesWritten < length)
                dbg (LOG_DEBUG, "Only %d bytes written of %d requested, attempt %d of %d", BytesWritten, length, i + 1, NumTries);

            length -= BytesWritten; // this is what breaks us out

            if (length == 0)
            {
                break;
            }
        } // end i

        if (i >= NumTries)
        {
            dbg (LOG_ERROR, "Could not write to '%s', Windows API WriteFile failed! Your device is probably *not* on this port, attempted %d times", port_name, i);
            ExitAndShowLog (1);
        }
    }

    if (BytesWritten == 0)
    {
        dbg (LOG_ERROR, "WriteFile failed, reported 0 bytes written");
        ExitAndShowLog (1);
        return 0;
    }

    /*
      if(BytesWritten!=length)
      {
        dbg(LOG_ERROR, "(P%"SIZE_T_FORMAT4") (H%"SIZE_T_FORMAT5") Tried to write %7d bytes, but only wrote %d bytes) ---------->\n\n\n",PacketsSent,CurrentDigestLocation,length,BytesWritten);
        ExitAndShowLog(1);
        return 0;
      }
    */
    if (CreateDigests)
    {
        sechsharm_sha256_init  (&context_per_packet);
        sechsharm_sha256_update (&context_per_packet, context_per_packet.leftover, & (context_per_packet.leftover_size), (unsigned char *) tx_buffer, BytesWritten);
        sechsharm_sha256_final (&context_per_packet, context_per_packet.leftover, & (context_per_packet.leftover_size), temp_hash_value);

        if (RawData == 0)
        {
            // offset 48 would be NOP, as in ?><data><NOP value, i.e. 48 is the beginning of the command
            CopyString (temp_buffer, tx_buffer, 0, 48, COMMAND_TRACE_BYTES_TO_RECORD, FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);
        }
        else
        {
            // Show some of the rawdata packet COMMAND_TRACE_BYTES_TO_RECORD/2 since each HEX value takes 2 bytes
            PrettyPrintHexValueIntoTempBuffer (pData, COMMAND_TRACE_BYTES_TO_RECORD / 2, 0, 32); // from, size, offset, maxlength
        }

        for (i = strlen (temp_buffer); i < COMMAND_TRACE_BYTES_TO_RECORD; i++)
            temp_buffer[i] = 0x20;  // string wasn't long enough, so pad with space

        AppendToBuffer (temp_buffer, " ---- ", FIREHOSE_TX_BUFFER_SIZE); // 0 to 37 is now full

        PrettyPrintHexValueIntoTempBuffer (temp_hash_value, 32, strlen (temp_buffer), 32); // from, size, offset, maxlength
        StripNewLines (temp_buffer, FIREHOSE_TX_BUFFER_SIZE, 0, strlen (temp_buffer) );


        if (createcommandtrace)
        {
            fc = ReturnFileHandle (CommandTraceName, strlen (CommandTraceName), "a" ); // will exit if not successful

            if ( CurrentDigestLocation == (MaxNumDigestsPerTable - 1) )
            {
                // this condition is when we would send a HASH table, so simulate this
                fprintf (fc, "(P%"SIZE_T_FORMAT4") (H%"SIZE_T_FORMAT5") (%7d bytes)                   NEW HASH TABLE SENT HERE (%3i)                 ---- XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n", PacketsSent, CurrentDigestLocation, (unsigned int) fh.attrs.MaxDigestTableSizeInBytes, HashTablesSent);
                HashTablesSent++;
                PacketsSent++;
                CurrentDigestLocation = 0;
            }

            fprintf (fc, "(P%"SIZE_T_FORMAT4") (H%"SIZE_T_FORMAT5") (%7d bytes) %s\n", PacketsSent, CurrentDigestLocation, BytesWritten, temp_buffer);
            fclose (fc);
            fc = NULL;
        }

        // BINARY FILE
        //if(strlen(SignedDigestTableFilename)>0)
        if (CreateDigests)
        {
            ft = ReturnFileHandle ("DIGEST_TABLE.bin", strlen ("DIGEST_TABLE.bin"), "ab" ); // will exit if not successful

            if (ft != NULL)
            {
                fwrite (temp_hash_value, 32, 1, ft);
                fclose (ft);
                ft = NULL;
            }
        }
    }


    PacketsSent++;
    CurrentDigestLocation++;

    return BytesWritten;
}

unsigned int ReadPort (unsigned char *pData, unsigned int length, unsigned int MaxLength)
{
    unsigned int BytesRead = 0;
    SIZE_T i;
    char answer = 'y';

    if (length > MaxLength)
    {
        length = MaxLength;
        //dbg(LOG_ERROR, "In ReadPort and length %d bytes is greater than buffer size of %d bytes",length,MaxLength);
        //ExitAndShowLog(1);
    }

    if (Interactive)
    {
        printf ("\n\n*INTERACTIVE* Press any key to attempt to **READ** %d bytes (q to quit interactive): ", length);
        answer = getchar();
        fseek (stdin, 0, SEEK_END); // get rid of extra \n keys

        if (answer == 'q')
            Interactive = 0;

        printf ("\n\n");
    }
    else
        dbg (LOG_DEBUG, "CharsInBuffer=%d Trying to read from USB %ld bytes", CharsInBuffer, length);

    for (i = 0; i < NumTries; i++)
    {
#ifdef _MSC_VER // i.e. if compiling under Windows

        if (!ReadFile (port_fd, pData, length, (LPDWORD) &BytesRead, NULL) )
        {
            // to be here means we didn't read as much as we wanted
            pData += BytesRead;
        }

        length -= BytesRead;

#else
        fd_set rfds;
        struct timeval tv;
        int retval;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        FD_ZERO (&rfds);
        FD_SET (port_fd, &rfds);

        retval = select (port_fd + 1, &rfds, NULL, NULL, &tv);

        if (retval == 0)
        {
            dbg (LOG_ERROR, "Timeout - no response trying to read from target\n\n");
            return 0;
        }


        BytesRead = read (port_fd, pData, length);

        if (BytesRead < 0)
        {
            dbg (LOG_ERROR, "Could not read from '%s', ret=0x%X\n", pData, BytesRead);
            //return 1;
        }

#endif


        if (BytesRead > 0)
        {
            break;
        }

        if (NumTries > 1000)
        {
            sleep (1); // to be here means user requested benchmarking, and thus need to give the target much more time
        }

    } // end i

    if (i >= NumTries)
    {
        dbg (LOG_ERROR, "Could not read from '%s', Windows API ReadFile failed! Your device is probably *not* on this port\n\n", port_name);
        ExitAndShowLog (1);
    }

    dbg (LOG_DEBUG, "CHANNEL DATA (%ld bytes) <-- TARGET to HOST", BytesRead);

    return BytesRead;
}

unsigned int OpenPort (char *pData)
{
    struct timeval time_start, time_end;
    double Elapsed;
    gettimeofday (&time_start, NULL);

    if (pData == NULL || strlen (pData) == 0)
    {
        dbg (LOG_ERROR, "Port to open was not specified. Please use --port= option\n\n");
        ExitAndShowLog (1);
    }

#ifdef _MSC_VER // i.e. if compiling under Windows
    port_fd = //CreateFileA(  (LPCSTR)pData,
        CreateFileA (  (LPCSTR) pData,
                       GENERIC_READ | GENERIC_WRITE,
                       0,
                       NULL,
                       OPEN_ALWAYS,//OPEN_EXISTING,
                       0,              // FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
                       NULL);

    if (port_fd == INVALID_HANDLE_VALUE)
    {
        dbg (LOG_ERROR, "Failed to open com port '%s'", pData);
        return 1;
    }

#else
    port_fd = open (pData, O_RDWR | O_SYNC);

    if (port_fd == -1)
    {
        dbg (LOG_ERROR, "Could not open '%s'\n", pData);
        return 1;
    }

#endif

    dbg (LOG_DEBUG, "port_fd=0x%X\n", port_fd);


    gettimeofday (&time_end, NULL);
    Elapsed = ReturnTimeInSeconds (&time_start, &time_end);

    if (Elapsed >= ComPortOpenTimeout)
    {
        dbg (LOG_ERROR, "It took %16.8f seconds to open port. Which is longer than %.3f. This indicates your target is not stable", Elapsed, ComPortOpenTimeout);
        ExitAndShowLog (1);
    }
    else
        dbg (LOG_INFO, "Took %16.8f seconds to open port", Elapsed);

    return 0;
}

void ClosePort (void)
{
    if (port_fd)
    {
#ifdef _MSC_VER // i.e. if compiling under Windows
        CloseHandle (port_fd);
        port_fd = NULL;
#else
        close (port_fd);
        port_fd = 0;
#endif
    }



}

void printBuffer (unsigned char *buffer, unsigned int length, unsigned int MaxLength, const char *func_name, int line_number)
{
    char CurrentChar = 0;
    char szBuffer[1024];

    //unsigned char asciiRepresentation[rowLength + 1];
    //asciiRepresentation[rowLength] = '\0';
    unsigned int i, j = 0, k = 0, b = 0;

    if (VerboseLevel == LOG_ALWAYS)
        return; // user has requested loglevel=0


    if (length > MaxLength)
    {
        dbg (LOG_ERROR, "PRETTYPRINT %s:%d Buffer is %d bytes which is BIGGER than %d bytes\n", func_name, line_number, length, MaxLength);
        ExitAndShowLog (1);
    }

    //dbg(LOG_ONLY, "PRETTYPRINT %s:%d Buffer is %d bytes\n", func_name, line_number, length);
    dbg (LOG_ONLY, "PRETTYPRINT Buffer is %d bytes\n", length);

    if (length == 0)
    {
        return;
    }

    i = 0;

    while (1)
    {
        //printf("\n");
        for (j = 0; j < 16; j++)
        {
            if (j + i >= length)
            {
                break;
            }

            if (b)
            {
                printf ("\n1 i+j=%i", i + j);
            }

            CurrentChar = buffer[j + i];
            sprintf (&szBuffer[j * 3], "%02X ", (CurrentChar & 0xFF) );
        } // end j

        k = j * 3;

        while (16 - j > 0)
        {
            j++;

            if (b)
            {
                printf ("\n1 k=%i", k);
            }

            sprintf (&szBuffer[k], "   ");
            k += 3; // 1 for each space
        }

        for (j = 0; j < 16; j++)
        {
            if (j + i >= length)
            {
                break;
            }

            if (b)
            {
                printf ("\n2 i+j=%i", i + j);
            }

            if (b)
            {
                printf ("\nk+j=%i", k + j);
            }

            CurrentChar = buffer[j + i];

            if (CurrentChar >= 0x20 && CurrentChar <= 0x7E)
            {
                if (CurrentChar == '%')
                {
                    sprintf (&szBuffer[k + j], "%%%%");
                    k++;
                }
                else
                    sprintf (&szBuffer[k + j], "%c", CurrentChar); // this puts a null

            }
            else
                sprintf (&szBuffer[k + j], ".");
        } // end j

        dbg (LOG_ONLY, szBuffer);

        k += j;
        i += 16;

        if (i >= length)
        {
            break;
        }

    } // end while

    sprintf (szBuffer, "\n\n");
    dbg (LOG_ONLY, szBuffer);

    return;
}

void InitBufferWithXMLHeader (char *MyBuffer, SIZE_T Length)
{
    memset (MyBuffer, 0x0, Length);
    memscpy (MyBuffer, Length, xml_header, XML_HEADER_LENGTH);
    //MyBuffer[XML_HEADER_LENGTH] = '\0';
} // end void InitBufferWithXMLHeader(void)

void AppendToBuffer (char *MyBuffer, char *buf, SIZE_T MaxBufferSize)
{
    size_t SpaceLeft, CharsToAdd = 0;
    size_t Length;

    Length       = strlen ( (const char *) MyBuffer);
    CharsToAdd   = strlen ( (const char *) buf); // size of string to append

    //SpaceLeft = FIREHOSE_TX_BUFFER_SIZE - Length - XML_TAIL_LENGTH;      // tx_buffer is transmit_buffer of size FIREHOSE_TX_BUFFER_SIZE
    SpaceLeft = MaxBufferSize - Length - XML_TAIL_LENGTH;      // tx_buffer is transmit_buffer of size FIREHOSE_TX_BUFFER_SIZE


    if (CharsToAdd > SpaceLeft)
    {
        return;  // too big to add this
    }

    // NOTE: If you're *not* seeing your messages, increase the size of transmit_buffer[FIREHOSE_TX_BUFFER_SIZE]
    //       or break up your logging

    //memcpy((MyBuffer+Length), buf, CharsToAdd);
    memscpy ( (MyBuffer + Length), MaxBufferSize, buf, CharsToAdd); // memcpy
    MyBuffer[Length + CharsToAdd] = '\0';

} // end void AppendToBuffer(car *MyBuffer, char *buf)


unsigned int sendTransmitBufferBytes (SIZE_T Length)
{
    TestIfWeNeedToSendDigestTable();
    BytesWritten = WritePort ( (unsigned char *) tx_buffer, Length, FIREHOSE_TX_BUFFER_SIZE, 1 ); // 1 means it is RawData
    return BytesWritten;
}
unsigned int sendTransmitBuffer (void)
{
    SIZE_T BytesBeingSent;

    TestIfWeNeedToSendDigestTable();

    BytesBeingSent = strlen ( (const char *) tx_buffer);
//  dbg(LOG_DEBUG,"CHANNEL DATA (%ld bytes) - HOST TO TARGET -->\n",BytesBeingSent);

    BytesWritten = WritePort ( (unsigned char *) tx_buffer, BytesBeingSent, FIREHOSE_TX_BUFFER_SIZE, 0 ); // 0 means it is a string
    return BytesWritten;
}



#define MAX_TAG_ATTR_LENGTH 4096

char NewTagName[MAX_TAG_NAME_LENGTH + 1];


SIZE_T CopyString (char *Dest, char *Source, SIZE_T  Dstart, SIZE_T  Sstart, SIZE_T  length, SIZE_T DestSize, SIZE_T SourceSize)
{
    SIZE_T  i = Dstart + length;
    char Temp;

    // WARNING, don't use dbg(LOG_INFO,etc) in this function or you get infinite recurrsion!!
    // WARNING, don't use dbg(LOG_INFO,etc) in this function or you get infinite recurrsion!!
    // WARNING, don't use dbg(LOG_INFO,etc) in this function or you get infinite recurrsion!!

    //if(length>=6000)
    //  printf("\nlength=%"SIZE_T_FORMAT" CopyString String of length %"SIZE_T_FORMAT" and DESTINATION Array of length %"SIZE_T_FORMAT,length,Dstart+length-1,DestSize);

    if (length == 0)
    {
        // This is a case like filename="", and so user is copying a null string
        Dest[Dstart] = '\0';    // NULL
        return 1;
    }

    if (Dest == '\0')
    {
        printf ("CopyString Dest is NULL");  // Dest is null
        ExitAndShowLog (1);
    }

    if (Source == '\0')
    {
        printf ("CopyString Source is NULL");  // Source is null
        ExitAndShowLog (1);
    }

    if (Sstart + length > SourceSize)
    {
        printf ("CopyString Range of %"SIZE_T_FORMAT" exceeds Array of length %"SIZE_T_FORMAT, Sstart + length, SourceSize);
        //ExitAndShowLog(1);
        return 0;
    } // range to copy is beyond source string

    if (i > 0 && (i - 1) > DestSize)
    {
        printf ("\ni=%"SIZE_T_FORMAT", (i-1)=%"SIZE_T_FORMAT" and DestSize=%"SIZE_T_FORMAT, i, i - 1, DestSize);
        printf ("\nCopyString String of length %"SIZE_T_FORMAT" at offset %"SIZE_T_FORMAT" of *dest will exceed Array of length %"SIZE_T_FORMAT" by %"SIZE_T_FORMAT" bytes", length, Dstart, DestSize, (length + Dstart - 1) - DestSize);
        //ExitAndShowLog(1);
        return 0;
    } // string to copy over is too big for destination

    //printf("CopyString(0x%X, 0x%X, %"SIZE_T_FORMAT", %"SIZE_T_FORMAT", %"SIZE_T_FORMAT", %"SIZE_T_FORMAT", %"SIZE_T_FORMAT")",Dest,Source,Dstart,Sstart,length,DestSize,SourceSize);
    if (Source == Dest)
        return length;  // user passed same buffer as source and destination, therefore nothing to copy

    for (i = 0; i < length; i++)
    {
        Temp = * (Source + Sstart + i);

        * (Dest + Dstart + i) = Temp;
    } // end i

    * (Dest + Dstart + i) = '\0'; // NULL

    return length;
}

char * RemoveBannedChars (char *p)
{
    char *pOrig = p;

    while (*p != '\0') //NULL)
    {
        if ( *p == '"' || *p == '<' || *p == '>')
            *p = '_';

        p++;
    }

    return pOrig;
}

/*
char * RemovePossibleDecimalPoint(char *p)
{
    char *pOrig = p;

    while(*p!='\0')
    {
        if( *p=='.')
        {
            *p = '\0'; // replace with NULL, number ends here
            break;
        }

         p++;
    }

    return pOrig;
}
*/


SIZE_T stringToNumber (const char* buffer, boolean *retval)
{
    char c;
    SIZE_T number = 0, ScaleFactor = 10;
    int i = 0, Length = strlen (buffer);
    int sign = 1;

    if (NULL == retval)
        return number;

    *retval = FALSE;    // assume false

    if (*buffer == '\0' || Length > 19)
        return number;
    i = 0;
    /* Check if the number is negative. */
    if(buffer[0] == '-')
    {
      sign = -1;
      i++;
    }
    for (; i < Length; i++)
    {
        c = buffer[i];

        if (c >= '0' && c <= '9')
            number = (number * ScaleFactor) + (c - 0x30); // ScaleFactor = 10
        else if (c >= 'a' && c <= 'f')
            number = (number * ScaleFactor) + (c - 87); // i.e. 'a' = 97, so 97-87 = 10
        else if (c >= 'A' && c <= 'F')
            number = (number * ScaleFactor) + (c - 55); // i.e. 'A' = 65, so 65-55 = 10
        else if (c == ',')
            continue; // user did this 12,345
        else if (c == 'x' || c == 'X')
        {
          /* user did 0x or 0X or any other weird combination (1xA, 23x0, 456xB,
             not robust but simple and works for most cases) */
            ScaleFactor = 16; // this is a hex value
            continue;
        }
        else if (c == '.')
            break;    // user did this 12.3, so return 12
        else
            return 0; // user provided something unexpected, return 0
    }

    *retval = TRUE; // made it this far, all is well

    return sign * number;
}

int ReturnNumAttributes (void)
{
    return (int) (sizeof (AllAttributes) / sizeof (struct Attributes_Struct) );
}

int ReturnAttributeLocation (char *NewAttrName)
{
    volatile SIZE_T i;
    SIZE_T StringLength1 = 0, StringLength2 = 0;

    StringLength1 = strlen ( (const char *) NewAttrName);

    for (i = 0; i < (SIZE_T) ReturnNumAttributes(); i++)
    {
        StringLength2 = strlen ( (const char *) AllAttributes[i].Name);

        if (StringLength1 < StringLength2)
            StringLength1 = StringLength2;  // want the biggest number

        if ( strncasecmp (AllAttributes[i].Name, NewAttrName, StringLength1) == 0)
        {
            return i;
        }
    }

    return -1;
}

int AssignAttribute (char *NewAttrName, char *NewAttrValue, SIZE_T NewAttrValueLen)
{
    volatile SIZE_T  i, j = 0, Matched = 0;
    SIZE_T TempValue;
    //printf("\n\nIn AssignAttribute(%s,%s)\n",NewAttrName,NewAttrValue);
    //logMessage("In AssignAttribute(%s,%s) and fh.attrs.Verbose=0x%X",NewAttrName,NewAttrValue,fh.attrs.Verbose);

    // Get attribute location
    i = ReturnAttributeLocation (NewAttrName);

    if (i == -1)
    {
        if (fh.attrs.Verbose == TRUE)
            dbg (LOG_INFO, "IGNORING UNRECOGNIZED Attribute '%s' with value '%s'", NewAttrName, NewAttrValue);

        return 0;
    }

    // To be this far we matched an attribute

    //if(fh.attrs.Verbose == TRUE)
    //    logMessage("Recognized Attribute Number %d ('%s') with value '%s' type is '%c'",i,NewAttrName,NewAttrValue,AllAttributes[i].Type);
    if ( CopyString (AllAttributes[i].Raw, NewAttrValue, 0, 0, strlen (NewAttrValue), sizeof (AllAttributes[i].Raw), NewAttrValueLen) == 0)
    {
        dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into AllAttributes[%i].Raw of length %i for fh.attrs.%s", NewAttrValue, strlen (NewAttrValue), i, sizeof (AllAttributes[i].Raw), NewAttrName);
        return 1;
    }

    if ( AllAttributes[i].Type == 'i' || AllAttributes[i].Type == 'w' || AllAttributes[i].Type == 'n' || AllAttributes[i].Type == 'b')
    {
        // to be here means attribute is some sort of number (not a string or a complex variable such as "NUM_DISK_SECTORS-33."
        boolean num_conversion;

        TempValue = stringToNumber ( (const char *) AllAttributes[i].Raw, &num_conversion);

        if (FALSE == num_conversion)
        {
            dbg (LOG_INFO, "Call to stringToNumber failed on attribute '%s' with value '%s'", NewAttrName, AllAttributes[i].Raw);
            return 1; // error
        }

        // Enforce bounds checking on the number we just converted
        // When it's handleFirmwareWrite we allow SECTOR_SIZE_IN_BYTES=1, so don't enforce any rules if it's that
        if (CurrentHandlerFunction != handleFirmwareWrite)
        {
            if ( (TempValue % AllAttributes[i].MultipleOf != 0) )
            {
                dbg (LOG_INFO, "Attribute '%s' is not a multiple of %i", NewAttrName, AllAttributes[i].MultipleOf);
                return 1; // error
            }
        }

        if ( AllAttributes[i].Max != 0 )
        {
            // Need to check against max value
            if ( TempValue > AllAttributes[i].Max )
            {
                dbg (LOG_INFO, "Attribute '%s' is larger than %i", NewAttrName, AllAttributes[i].Max);
                return 1; // error
            }
        }

        if ( AllAttributes[i].Min != 0 )
        {
            // Need to check against min value
            if ( TempValue < AllAttributes[i].Min )
            {
                dbg (LOG_INFO, "Attribute '%s' is smaller than %i", NewAttrName, AllAttributes[i].Min);
                return 1; // error
            }
        }

        // Now assign to the actual fh.attrs.variable

        if (AllAttributes[i].Type == 'w')
            * ( (unsigned int *) AllAttributes[i].pValue) = (unsigned int) TempValue;
        else if (AllAttributes[i].Type == 'n')
            * ( (short *) AllAttributes[i].pValue)        = (short) TempValue;
        else if (AllAttributes[i].Type == 'b')
            * ( (char *) AllAttributes[i].pValue)         = (char) TempValue;
        else
            * ( (SIZE_T *) AllAttributes[i].pValue)       = (SIZE_T) TempValue;

    }
    else if ( AllAttributes[i].Type == 's' )
    {
        if ( CopyString (AllAttributes[i].pStr, NewAttrValue, 0, 0, strlen (NewAttrValue), AllAttributes[i].SizeOfpStr, NewAttrValueLen)  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string '%s' into AllAttributes[%"SIZE_T_FORMAT"].pStr", NewAttrValue, i);
            return 1;
        }
    }

    // Here show user what just happened
    //if(fh.attrs.Verbose == TRUE)
    //  dbg(LOG_INFO, "fh_loader Recognized Attribute Number %d ('%s') with value '%s' type is '%c'",i,NewAttrName,NewAttrValue,AllAttributes[i].Type);

    // else we have the special 'x' case, handled later if Assigned has been set
    j = i; // for breakpoint
    AllAttributes[i].Assigned = 1;

    return 0;
}

SIZE_T  DetermineTag (char *Packet, SIZE_T CurrentPacketLoc, SIZE_T MaxPacketSize);
SIZE_T  DetermineAttributes (char *Packet, SIZE_T CurrentPacketLoc, SIZE_T MaxPacketSize);
SIZE_T  GetStringFromXML (char *Packet, SIZE_T  CurrentPacketLoc, SIZE_T PacketLength);

char * RemoveEverythingButTags (char *Packet, SIZE_T  CurrentPacketLoc, SIZE_T MaxPacketSize)
{
    SIZE_T  PacketLength, PacketLoc = 0, i, LengthOfXMLHeader = 0;
    char *pch;
    long FoundLeftBrace = -1, FoundRightBrace = -1, FoundForwardSlash = -1, Found = 0;
    char CurrentTagName[MAX_TAG_NAME_LENGTH + 1];

    if (CurrentPacketLoc >= MaxPacketSize)
        return Packet; // nothing to do

    ThisXMLLength = 0;

    if (CurrentPacketLoc > 0)
        memset (Packet, 0x20, CurrentPacketLoc);

    PacketLength = strlen ( (const char *) Packet);

    if (CurrentPacketLoc >= PacketLength)
        return Packet; // nothing to do, we have run out of packet

    if (CharsInBuffer > 0 && Packet == ReadBuffer)
    {
        PacketLength = CharsInBuffer;
        Packet[CharsInBuffer] = '\0';
    }

    if (PacketLength > MaxPacketSize)
        return Packet; // should be impossible


    // For debugging, if something goes wrong, I need to see the XML I'm looking at
    if ( CopyString (temp_buffer2, Packet, 0, 0, PacketLength, FIREHOSE_TX_BUFFER_SIZE, PacketLength) == 0)
    {
        dbg (LOG_ERROR, "Couldn't backup Packet into temp_buffer2 before parsing!");
        ExitAndShowLog (1);
    }

    //-------------------------------------------------------------------------------------------------
    // Step 1 is nuke any comments in the file
    if (RemoveCommentsFromXMLFiles)
    {
        for (i = CurrentPacketLoc; i < PacketLength; i++)
        {
            if (Found == 0)
            {
                if (i + 3 < PacketLength)
                {
                    if (Packet[i] == '<' && Packet[i + 1] == '!' && Packet[i + 2] == '-' && Packet[i + 3] == '-') // protected by if(i+3<PacketLength) above
                    {
                        Found = 1;
                        Packet[i] = ' ';
                        Packet[i + 1] = ' ';
                        Packet[i + 2] = ' ';
                        Packet[i + 3] = ' ';
                        i += 3;
                    }
                }
            } // end of if(Found==0)
            else
            {
                // we are inside of a comment
                if (i + 2 < PacketLength)
                {
                    if (Packet[i] == '-' && Packet[i + 1] == '-' && Packet[i + 2] == '>') // protected by if(i+2<PacketLength) above
                    {
                        Found = 0;
                        Packet[i] = ' ';
                        Packet[i + 1] = ' ';
                        Packet[i + 2] = ' ';
                        i += 2;
                        continue;
                    }
                } // end if(i+2<PacketLength)

                Packet[i] = ' '; // remove since this is a comment
            }

            i = i;
        } // end for(i=CurrentPacketLoc;i<PacketLength;i++)
    } // end of if(RemoveCommentsFromXMLFiles)

    // At this point, XML file has all <!-- comments --> replaced by spaces
    //-------------------------------------------------------------------------------------------------


    // Now nuke the first <?xml version="1.0" ?>
    do
    {
        pch = strstr (Packet, "xml version");

        if (pch == NULL)
        {
            break;
        }

        // to be this far we found "xml version"
        // Now look slightly beyond that

        pch = strstr (pch, ">");      // is now pointing to the beginning of the file "><data><log etc"

        if (pch != NULL) //'\0')         // pch = "<configuration> <options> DOWNLOAD_PROTOCOL       = FIREHOSE etc etc
        {
            LengthOfXMLHeader = pch - &Packet[CurrentPacketLoc] + 1;

            if (ShowXMLFileInLog)
            {
                // Show user the packet we have
                if ( CopyString (temp_buffer, Packet, 0, CurrentPacketLoc, LengthOfXMLHeader, FIREHOSE_TX_BUFFER_SIZE, MaxPacketSize)  == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy string of length %"SIZE_T_FORMAT" bytes into temp_buffer", pch - &Packet[PacketLoc] + 1);
                    ExitAndShowLog (1);
                }
            }

            memset (&Packet[CurrentPacketLoc], 0x20, LengthOfXMLHeader); // this removes <?xml version="1.0" ?>
        }

        // We could have more than 1 XML file here
        pch = strstr (Packet, "xml version");

        if (pch != NULL)
        {
            PacketLength = pch - Packet; // now length updated to start of next XML
        }



        FoundRightBrace = -1;
        FoundLeftBrace  = -1;

        // find the tag name, i.e. <data>
        for (i = CurrentPacketLoc; i < PacketLength; i++)
        {
            if ( Packet[i] == '<' )
            {
                FoundLeftBrace = i;
            }

            if (FoundLeftBrace > 0)
                if ( Packet[i] == '>' )
                {
                    FoundRightBrace = i;
                    break;
                }

        } // end for

        if ( FoundRightBrace == -1 || FoundLeftBrace == -1)
        {
            dbg (LOG_ERROR, "Didn't fine either Left Brace or Right Brace. XML file is not formed correctly");
            ExitAndShowLog (1);
        }

        if ( (FoundRightBrace - FoundLeftBrace + 2) > MAX_STRING_SIZE - 1 )
        {
            dbg (LOG_ERROR, "Either closing tag is bigger than %d bytes, or the XML file is not formed correctly", MAX_STRING_SIZE);
            ExitAndShowLog (1);
        }


        memset (CurrentTagName, 0, sizeof (CurrentTagName) );

        if ( CopyString (CurrentTagName, Packet, 0, FoundLeftBrace, FoundRightBrace - FoundLeftBrace + 1, sizeof (CurrentTagName), MaxPacketSize)  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string of length %"SIZE_T_FORMAT" bytes into CurrentTagName", FoundRightBrace - FoundLeftBrace + 1);
            ExitAndShowLog (1);
        }

        // Here CurrentTagName most likely holds <data>

        CurrentTagName[0] = '/';              // CurrentTagName = "<data>" becomes "/data>" OR "<configuration>" becomes "/configuration>"

        // Now we are looking for the closing </data>
        pch = strstr (Packet, CurrentTagName);      // is now pointing to the beginning of the file

        if (pch == NULL) //'\0')
        {
            dbg (LOG_ERROR, "Could not find closing tag in file '%s'", CurrentTagName);
            dbg (LOG_ERROR, "Below is what I think I'm looking at, %i bytes", strlen (Packet) );
            //printf("length is %i",strlen(Packet));
            pch = strstr (Packet, "start_sector");
            pch = strstr (Packet, "filename");
            pch = strstr (Packet, "label");
            pch = strstr (Packet, "/data>");

            PRETTYPRINT ( (unsigned char*) temp_buffer2, FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);

            dbg (LOG_ALWAYS, "%s", temp_buffer2);

            ExitAndShowLog (1);
        }

        // To be this far I know I have at least 1 complete XML file

        ThisXMLLength = (pch - Packet) + strlen (CurrentTagName);

        // CLEAR anything after this to be safe
        if (ThisXMLLength < (SIZE_T) CharsInBuffer)
            memset (&Packet[CharsInBuffer], 0, MaxPacketSize - CharsInBuffer);
        else
            memset (&Packet[ThisXMLLength], 0, MaxPacketSize - ThisXMLLength);


        if (ShowXMLFileInLog)
        {

            if (Packet[CurrentPacketLoc + ThisXMLLength] == 0xA)
                ThisXMLLength++;  // to include the newline char 0xA


            // I can print the full packet here, copy it into temp_buffer
            if ( CopyString (temp_buffer, Packet, LengthOfXMLHeader, CurrentPacketLoc + LengthOfXMLHeader, ThisXMLLength - LengthOfXMLHeader, FIREHOSE_TX_BUFFER_SIZE, MaxPacketSize)  == 0)
            {
                dbg (LOG_ERROR, "Failed to copy string of length %"SIZE_T_FORMAT" bytes into temp_buffer", PacketLength - CurrentPacketLoc);
                ExitAndShowLog (1);
            }

            dbg (LOG_DEBUG, "XML FILE (%i bytes): CharsInBuffer=%i-%i=%i\n"
                 "-------------------------------------------------------------------------------------------\n"
                 "%s\n"
                 "-------------------------------------------------------------------------------------------\n"
                 , strlen (temp_buffer), CharsInBuffer, strlen (temp_buffer), CharsInBuffer - strlen (temp_buffer), temp_buffer);
        }

        memset (&Packet[FoundLeftBrace - 1], 0x20, strlen (CurrentTagName) + 1); // this takes out <configuration> OR <data> at the beginning of the file

        memset (&Packet[ThisXMLLength - strlen (CurrentTagName) - 1], 0x20, strlen (CurrentTagName) + 1); // this takes out </configuration> OR </data> at the end of the file


        break; // breaking the do loop

    }
    while (1); // end of looking for "xml version"

    return Packet;
}

void ResetAllAttributeAssignFlags (void)
{
    SIZE_T  i;

    for (i = 0; i < (SIZE_T) ReturnNumAttributes(); i++)
        AllAttributes[i].Assigned = 0;
}

SIZE_T  DetermineTag (char *Packet, SIZE_T  CurrentPacketLoc, SIZE_T MaxPacketSize)
{
    volatile SIZE_T  i = 0;
    SIZE_T  PacketLength;
    int CurrentTag = -1;
    char *pch;
    long FoundLeftBrace = -1, FoundRightBrace = -1, FoundForwardSlash = -1, Found = 0;

    if (CurrentPacketLoc >= MaxPacketSize)
    {
        dbg (LOG_ERROR, "CurrentPacketLoc>=MaxPacketSize");
        return 0; // out of packet
    }

    Packet = RemoveEverythingButTags (Packet, CurrentPacketLoc, MaxPacketSize); // When this func is done, only <response etc etc remains
    // Do strlen after RemoveEverythingButTags since the last "\n" can be replaced with a '\0'
    PacketLength = ThisXMLLength;

    if (PacketLength == 0)
    {
        // this means it was a config.xml, i.e. multiple tags don't have the <?xml header, so above calls it length 0
        PacketLength = strlen ( (const char *) Packet);

        if (PacketLength == 0)
        {
            dbg (LOG_ERROR, "No XML detected?? Something has gone wrong. Either an XML file you provided has syntax errors, or your target responded with garbage. Cannot continue. Log will give more information");
            ExitAndShowLog (1);
        }
    }

    if (CurrentPacketLoc >= PacketLength)
    {
        // we have run out of packet
        return CurrentPacketLoc;
    }

    if (PacketLength > MaxPacketSize)
    {
        dbg (LOG_ERROR, "PacketLength>MaxPacketSize");
        return 0; // should be impossible
    }

    ResetAllAttributeAssignFlags();

    // This chomps up empty space till we hit a tag
    while ( IsEmptySpace ( Packet[CurrentPacketLoc] ) )
    {
        CurrentPacketLoc++;

        if (CurrentPacketLoc >= PacketLength)
        {
            dbg (LOG_ERROR, "XML packet not formed correctly. Ran out of room looking for TAG");
            return 0;
        }
    }

    if (Packet[CurrentPacketLoc] != '<')
    {
        dbg (LOG_ERROR, "XML not formed correctly. Expected a &lt; character at loc %d", CurrentPacketLoc);
        return 0; // not formed correctly
    }

    CurrentPacketLoc++;    // move past the above opening <

    // Could be one solid tag like <options>, test by looking for spaces *before* seeing a '>'
    FoundRightBrace = -1;

    for (i = CurrentPacketLoc; i < PacketLength; i++)
    {
        if ( Packet[i] == '>' )
        {
            FoundRightBrace = i;
            break;
        }

        if ( Packet[i] == ' ' )
        {
            break;
        }
    }

    if (FoundRightBrace > 0)
    {
        // this is something like <options> some text </options>
        ParseAttributes = 0;  // need to parse the strings

        CopyString (NewTagName, Packet, 0, CurrentPacketLoc,   FoundRightBrace - CurrentPacketLoc, sizeof (NewTagName), MaxPacketSize ); // NewTagName has the TAG now
        RemoveBannedChars (NewTagName); // chars like " < > are replaced by _

        CurrentPacketLoc = FoundRightBrace + 1; // The +1 to get past the >

    }
    else
    {
        // or could be tag with attributes like <program filename
        ParseAttributes = 1;

        pch = strstr (&Packet[CurrentPacketLoc], " ");         // Find a space after the TAG name

        if (pch == NULL) //'\0')   // if null, XML is not formed correctly
        {
            dbg (LOG_ERROR, "1) XML not formed correctly. Expected one SPACE character at loc %d", CurrentPacketLoc);
            return 0; // not formed correctly
        }

        if ( pch - &Packet[CurrentPacketLoc] > MAX_TAG_NAME_LENGTH )
        {
            dbg (LOG_ERROR, "The XML TAG is too long (%d chars) only %d chars allowed", (pch - &Packet[CurrentPacketLoc]), MAX_TAG_NAME_LENGTH);
            return 0; // not formed correctly
        }


        CopyString (NewTagName, Packet, 0, CurrentPacketLoc,   pch - &Packet[CurrentPacketLoc], sizeof (NewTagName), MaxPacketSize ); // NewTagName has the TAG now
        RemoveBannedChars (NewTagName); // chars like " < > are replaced by _

        CurrentPacketLoc += strlen ( (const char *) NewTagName);
        CurrentPacketLoc++;    // move past the above opening space we confirmed exists above

    }

    for (i = 0; i < sizeof (firehose_tag_data) / sizeof (firehose_tag_data_t); i++)
    {
        if (strncasecmp (firehose_tag_data[i].tag_name, NewTagName, strlen ( (const char *) NewTagName) ) == 0)
        {
            CurrentTag             = i;
            CurrentHandlerFunction = firehose_tag_data[i].handler;
            break;
        }
    } // end i

    if (CurrentTag == -1)
    {
        dbg (LOG_ALWAYS, "**INFO Unrecognized tag '%s', so I will simply look for ACK or NAK", NewTagName);
        CurrentHandlerFunction = handleUnrecognized;
        //return 0;
    }

    // The above got the <TAG>, accessible like this firehose_tag_data[CurrentTag].handler and firehose_tag_data[CurrentTag].tag_name

    return CurrentPacketLoc;

} // end DetermineTag(char *Packet, SIZE_T  PacketLoc)


SIZE_T  GetStringFromXML (char *Packet, SIZE_T  CurrentPacketLoc, SIZE_T PacketLength)
{
    char *pch;
    // to be here means we have determined it's a bunch of string value pairs
    // DOWNLOAD_PROTOCOL       = FIREHOSE
    // LOAD_RAW_PROGRAM_FILES  = true
    // LOAD_PATCH_PROGRAM_FILES= true
    // </options>

    //MyStringPairs[MAX_STRING_PAIR_SIZE]
    pch = strstr (&Packet[CurrentPacketLoc], "<");         // Find an opening brace


    if (strncasecmp (NewTagName, "file_mappings", strlen ("file_mappings") ) == 0)
    {
        // This is a huge list of file mappings but not needed for programmer
        CurrentPacketLoc += (pch - &Packet[CurrentPacketLoc]) + 1; // +1 to get past the > symbol
        CurrentPacketLoc += strlen ("</file_mappings");      // get past this

        // This chomps up empty space till we hit a tag
        while ( IsEmptySpace ( Packet[CurrentPacketLoc] ) )
        {
            CurrentPacketLoc++;

            if (CurrentPacketLoc >= PacketLength)
            {
                // probably ran out of file, this is expected
                break;
            }
        }

        return CurrentPacketLoc;
    }

    if (pch - &Packet[CurrentPacketLoc] > MAX_STRING_PAIR_SIZE)
    {
        dbg (LOG_ERROR, "Too many string pairs. Size of %d is bigger than %d", pch - &Packet[CurrentPacketLoc], MAX_STRING_PAIR_SIZE);
        ExitAndShowLog (1);
    }

    CopyString (MyStringPairs, Packet, 0, CurrentPacketLoc,   pch - &Packet[CurrentPacketLoc], sizeof (MyStringPairs), MAX_STRING_PAIR_SIZE ); // NewAttrValue has the ATTR value now

    pch = strstr (&Packet[CurrentPacketLoc], ">");         // Find closing brace

    if (pch - &Packet[CurrentPacketLoc] > MAX_STRING_PAIR_SIZE)
    {
        dbg (LOG_ERROR, "Too many string pairs. Size of %d is bigger than %d", pch - &Packet[CurrentPacketLoc], MAX_STRING_PAIR_SIZE);
        ExitAndShowLog (1);
    }

    return CurrentPacketLoc + (pch - &Packet[CurrentPacketLoc]) + 1; // +1 to get past the > symbol
}

SIZE_T  DetermineAttributes (char *Packet, SIZE_T  CurrentPacketLoc, SIZE_T MaxPacketSize)
{
    SIZE_T  i = 0, AttributesFound = 0;
    SIZE_T  PacketLength;

    char NewAttrName[MAX_TAG_ATTR_LENGTH];
    char NewAttrValue[MAX_TAG_ATTR_LENGTH];
    char *pch;

    if (CurrentPacketLoc >= MaxPacketSize)
        return 0; // ran out of packet

    // step 4. Get attributes
    PacketLength    = strlen ( (const char *) Packet);
    // Packet[CurrentPacketLoc] should be at the first letter of the attribute now
    // They all look like this attribute1="value1" attribute2="value2"
    // Meaning only spaces seperate them

    if (PacketLength > MaxPacketSize)
        return 0; // should be impossible

    AttributesFound = 0;

    while (CurrentPacketLoc < PacketLength)
    {
        pch = strstr (&Packet[CurrentPacketLoc], "=");         // Find an equals sign

        if (pch == NULL) //'\0')   // if null, we didn't find it
        {
            dbg (LOG_ERROR, "XML not formed correctly. Could not find '=' character");
            return 0; // not formed correctly
        }

        if ( pch - &Packet[CurrentPacketLoc] > MAX_TAG_ATTR_LENGTH )
        {
            dbg (LOG_ERROR, "The value for XML attribute is too long (%d chars) only %d chars allowed", (pch - &Packet[CurrentPacketLoc]), MAX_TAG_ATTR_LENGTH);
            return 0; // not formed correctly
        }

        CopyString (NewAttrName, Packet, 0, CurrentPacketLoc,   pch - &Packet[CurrentPacketLoc], sizeof (NewAttrName), MaxPacketSize ); // NewAttrName has the ATTR now

        RemoveBannedChars (NewAttrName); // chars like " < > are replaced by _

        TrimTrailingWhiteSpaceFromStringVariable (NewAttrName);

        CurrentPacketLoc += strlen ( (const char *) NewAttrName);

        CurrentPacketLoc++;    // move past the = sign

        if (Packet[CurrentPacketLoc] != '"')
        {

            dbg (LOG_ERROR, "XML not formed correctly!!\n\n"
                 "\nYou probably have *extra* spaces near %s in your XML file\n\n"
                 "** You need to manually edit this file and correct this **\n\n\n"
                 "NOTE: The XML parsing is limited.\n\nattribute=\"value\"   <--- This program expects this\nattribute = \"value\" <--- Not this (Notice the *extra* spaces here)\n\n",
                 NewAttrName);

            ExitAndShowLog (1); // not formed correctly
        }

        CurrentPacketLoc++;    // Before "value" and now we move past the " sign, all that is left now is the actual value"

        pch = strstr (&Packet[CurrentPacketLoc], "\"");        // Find closing "

        if (pch == NULL) //'\0')   // if null, we didn't find it
        {
            dbg (LOG_ERROR, "XML not formed correctly. Expected one &quot; character at loc %d", CurrentPacketLoc);
            return 0; // not formed correctly
        }

        if ( pch - &Packet[CurrentPacketLoc] > MAX_TAG_ATTR_LENGTH )
        {
            dbg (LOG_ERROR, "The value for XML attribute '%s' is too long (%d chars) only %d chars allowed", NewAttrName, (pch - &Packet[CurrentPacketLoc]), MAX_TAG_ATTR_LENGTH);
            return 0; // not formed correctly
        }

        CopyString (NewAttrValue, Packet, 0, CurrentPacketLoc,   pch - &Packet[CurrentPacketLoc], sizeof (NewAttrValue), MaxPacketSize ); // NewAttrValue has the ATTR value now
        RemoveBannedChars (NewAttrValue); // chars like " < > are replaced by _

        AttributesFound++;

        // FIGURE OUT WHICH ATTRIBUTE AND ASSIGN VALUE

        if ( AssignAttribute (NewAttrName, NewAttrValue, sizeof (NewAttrValue) ) != 0 )
        {
            dbg (LOG_ERROR, "AssignAttribute failed. Could not assign '%s' the value '%s'", NewAttrName, NewAttrValue);
            ExitAndShowLog (1); // not formed correctly
        }

        CurrentPacketLoc += strlen ( (const char *) NewAttrValue);
        CurrentPacketLoc++;    // move past the closing quote " sign

        while ( IsEmptySpace ( Packet[CurrentPacketLoc] ) )
        {
            CurrentPacketLoc++;

            if (CurrentPacketLoc >= PacketLength)
            {
                dbg (LOG_ERROR, "XML packet not formed correctly. Ran out of room looking for ATTRIBUTES");

                return 0;
            }
        }


        if (Packet[CurrentPacketLoc] == '/')
        {
            // This is the END of the packet
            return CurrentPacketLoc + 2; // +2 gets us past the />
        }
    } // end of while(CurrentPacketLoc < PacketLength)

    return 0;       // to be here means we ran out of packet

} // end DetermineAttributes(char *Packet, SIZE_T CurrentPacketLoc)

static firehose_error_t handlePower()
{
    char Answer = 'y';

    if (PromptUser)
    {
        printf ("\n\nSend RESET to target with DelayInSeconds=\"2\" ? (y|n):");
        Answer = getchar();
        fseek (stdin, 0, SEEK_END); // get rid of extra \n keys
    }

    if (Answer != 'y')
    {
        dbg (LOG_INFO, "Not sending RESET at users request");
        return FIREHOSE_SUCCESS;
    }


    dbg (LOG_INFO, "Sending <power>");
    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if (GotACK)
        dbg (LOG_DEBUG, "Got the ACK");
    else
        return FIREHOSE_ERROR;

    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleSetBootableStorageDrive()
{
    dbg (LOG_INFO, "Sending <setbootablestoragedrive>");
    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if (GotACK)
        dbg (LOG_DEBUG, "Got the ACK");
    else
        return FIREHOSE_ERROR;

    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleConfigure()
{
    char *pch;
    SIZE_T OrigMaxPayloadSizeToTargetInBytes = fh.attrs.MaxPayloadSizeToTargetInBytes;
    // NOTE: This packet can be sent more than once until negotion is done

    memset (last_log_value, 0x0, strlen (last_log_value) );

    do
    {

        dbg (LOG_INFO, "Sending <configure>");

        // tx_buffer already holds the XML file
        sendTransmitBuffer();

        if (Simulate)
        {
            InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);

            //ReadBuffer[strlen((const char *)ReadBuffer)-1]='\0';  // HACK to remove \n above for testing

            AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);

            //AppendToBuffer(ReadBuffer,"<data>",MAX_READ_BUFFER_SIZE); // HACK TO TEST
            if (fh.attrs.MaxPayloadSizeToTargetInBytes > fh.attrs.MaxPayloadSizeToTargetInBytesSupported)
            {
                AppendToBuffer (ReadBuffer, "<response value=\"NAK\" ", MAX_READ_BUFFER_SIZE);
                sprintf (temp_buffer, "MaxPayloadSizeToTargetInBytes=\"%"SIZE_T_FORMAT"\" ", fh.attrs.MaxPayloadSizeToTargetInBytesSupported);
                AppendToBuffer (ReadBuffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
                sprintf (temp_buffer, "MaxPayloadSizeToTargetInBytesSupported=\"%"SIZE_T_FORMAT"\" ", fh.attrs.MaxPayloadSizeToTargetInBytesSupported);
                AppendToBuffer (ReadBuffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
            }
            else
            {
                AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
                sprintf (temp_buffer, "MaxPayloadSizeToTargetInBytes=\"%"SIZE_T_FORMAT"\" ", fh.attrs.MaxPayloadSizeToTargetInBytes);
                AppendToBuffer (ReadBuffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
                sprintf (temp_buffer, "MaxPayloadSizeToTargetInBytesSupported=\"%"SIZE_T_FORMAT"\" ", fh.attrs.MaxPayloadSizeToTargetInBytesSupported);
                AppendToBuffer (ReadBuffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
            }

            AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
            //AppendToBuffer(ReadBuffer,"/></data>",MAX_READ_BUFFER_SIZE);  // HACK to test targets that don't put \n at the end
            CharsInBuffer = strlen (ReadBuffer);
            //sprintf(temp_buffer,"value=\"%s\" ",fh.attrs.value);
            //AppendToBuffer(ReadBuffer,temp_buffer);

        }

        GetNextPacket();  // this will set all variables, including GotACK

        // Target can ACK or NAK my configure packet. As it is setup now, all that changes is that Target will have *set* variables on PC side
        // That is fh_loader.exe will have it's MaxPayloadSizeToTargetInBytes *set* to whatever target has

        dbg (LOG_INFO, "fh.attrs.MaxPayloadSizeToTargetInBytes = %"SIZE_T_FORMAT, fh.attrs.MaxPayloadSizeToTargetInBytes);
        dbg (LOG_INFO, "fh.attrs.MaxPayloadSizeToTargetInBytesSupported = %"SIZE_T_FORMAT, fh.attrs.MaxPayloadSizeToTargetInBytesSupported);

        MaxBytesToReadFromUSB             = fh.attrs.MaxPayloadSizeFromTargetInBytes;


        //last_log_value
        if (!GotACK)
        {
            // to be here means a NAK came. Target is rejecting something
            if (UsingValidation)
            {
                dbg (LOG_ERROR, "Got a NAK. Target *did not* like your <configure>. This is most likely due to one of your parameters (attributes) not matching the target!"
                     "\n\nFor example, this can happen if you have specified in <configure> MaxPayloadSizeToTargetInBytes=\"16384\"\nbut target has specified in <response> MaxPayloadSizeToTargetInBytes=\"8192\" "
                     "\n\nNOTE: For VIP your <configure> parameters must match the targets <response>. Check the LOG and see the targets <response>. You manually need to check the LOG"
                     "\n\nThus for example, you need to change your <configure> MaxPayloadSizeToTargetInBytes=\"8192\", by doing --maxpayloadsizeinbytes=8192"
                     "\n\nOR, you must regenerate your digests tables, i.e. redo the 'dry run' with --createdigests"
                     "\n\nReset your target to continue"
                     "\nReset your target to continue"
                     "\nReset your target to continue"
                     "\n\nNOTE: This is *not* a VIP error nor is it a hash mismatch. This is your parameters don't match the targets, so we can't continue\n\n");

                ExitAndShowLog (1);
            }

            // to be this far it's not VIP

            if (AllAttributes[1].Assigned == 0)
            {
                // if target didn't assign any variables then this is an indication that it failed on some storage call
                // and thus didn't ReadBuffer is like <response value="NAK" />   i.e. it didn't make it far enough to fill in attrs
                dbg (LOG_INFO, "Something failed. The target rejected your <configure>. Please inspect log for more information");
                ExitAndShowLog (1); // these match so this must be a true failure, so quit
            }

            pch = strstr (last_log_value, "ERROR");

            // to be this far we have a NAK and not using VIP, so maybe we can recover
            if (pch != NULL)
            {
                dbg (LOG_INFO, "Something failed with <configure>. This is usually related to STORAGE '%s' *not* coming up", fh.attrs.MemoryName);
                ExitAndShowLog (1); // these match so this must be a true failure, so quit
            }
            else
            {
                if (BytesRead == 0)
                {
                    dbg (LOG_ERROR, "Your target is NOT responding!!");
                    ExitAndShowLog (1);
                }

                dbg (LOG_INFO, "Target returned NAK for your <configure> but it does not seem to be an error. This is ok, fh_loader.exe attributes updated");
            }
        }


        if (GotACK)
            dbg (LOG_DEBUG, "Got the ACK for the <configure>");

        if (OrigMaxPayloadSizeToTargetInBytes != fh.attrs.MaxPayloadSizeToTargetInBytes)
        {
            if (UsingValidation)
            {
                dbg (LOG_ERROR, "You are using VIP and target wants MaxPayloadSizeToTargetInBytes=%"SIZE_T_FORMAT" and you have --maxpayloadsizeinbytes=%"SIZE_T_FORMAT, fh.attrs.MaxPayloadSizeToTargetInBytes, OrigMaxPayloadSizeToTargetInBytes);
                ExitAndShowLog (1); // these match so this must be a true failure, so quit
            }
        }

        if (CreateDigests)
            break;  // do not negotiate during the dry run

        if (UsingValidation)
            break;  // do not negotiate when using VIP, since the "dry run" would have been made with this value

        // NOTE: above I checked that host and target agree


        // To be this far it is not a "dry run" and we are not using VIP
        // Therefore it is possible the target can handle a bigger payload size

        if (fh.attrs.MaxPayloadSizeToTargetInBytes == fh.attrs.MaxPayloadSizeToTargetInBytesSupported)
            break;  // we are done, target does not offer a bigger payload size


        // to be this far means we don't agree on payload size yet
        fh.attrs.MaxPayloadSizeToTargetInBytes = fh.attrs.MaxPayloadSizeToTargetInBytesSupported;
        LoadConfigureIntoTXBuffer();

    }
    while (1);  // end do

    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleLog()
{
    printf ("\n");
    dbg (LOG_INFO, "TARGET SAID: '%s'", fh.attrs.value);
    CopyString (last_log_value, fh.attrs.value, 0, 0, strlen (fh.attrs.value), FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);


    if (UsingValidation)
    {
        char *pch;

        pch = strstr (last_log_value, "PACKET_HASH_MISMATCH");

        if (pch != NULL)
        {
            dbg (LOG_ERROR, "The SHA256 digest of the last packet sent by HOST PC *does not* match what target expected"
                 "\n\nThis means either"
                 "\n\t1. You sent the wrong signed digest table, i.e. --signeddigests=DigestsToSign.bin.mbn"
                 "\n\t2. You have changed for example, a parameter such as --maxpayloadsizeinbytes=16384 whereas when you did the \"dry run\" it was --maxpayloadsizeinbytes=8192 "
                 "\n\t3. Possibly one of the files have changed since when you ran the \"dry run\". If you saved the original \"command_trace.txt\", this can help in debugging"
                 "\n\nThere is no recovery from this. You need to reset your target OR send a different signed digest table --signeddigests=DigestsToSign2.bin.mbn\n");
            ExitAndShowLog (1);
        }
    }

    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleSearchPaths()
{
    char Key[MAX_STRING_SIZE] = {0} , Value[MAX_STRING_SIZE] = {0};
    SIZE_T PacketLoc = 0, PacketLength, i = 0, OnKey = 1, PairFound = 0;
    SIZE_T j, k;
    char c;

    // Strings in MyStringPairs will be like
    //  \\snowcone\builds672\TEST\M8994AAAAANLGD1000178.1\common\build\emmc
    //  \\snowcone\builds672\TEST\M8994AAAAANLGD1000178.1\common\build\emmc\bin\asic\

    PacketLength = strlen (MyStringPairs);

    while ( IsEmptySpace ( MyStringPairs[PacketLoc] ) )
    {
        PacketLoc++;

        if (PacketLoc >= PacketLength)
            break;  // out of packet, so get out of here
    }

    // I can use this function below, as long as the newlines are replaced by commas
    // num_search_paths = SplitStringOnCommas(MyArg,search_path,num_search_paths);

    while (PacketLoc < PacketLength)
    {
        if (MyStringPairs[PacketLoc] == 0xA)
            MyStringPairs[PacketLoc] = ',';

        PacketLoc++;
    }

    // MyStringPairs into search_path[MAX_XML_FILES][MAX_PATH_SIZE]
    num_search_paths = SplitStringOnCommas (MyStringPairs, sizeof (MyStringPairs), search_path, num_search_paths, MAX_XML_FILES, MAX_PATH_SIZE);

    search_path[num_search_paths][0] = '\0'; // j is how many strings were valid, make sure last one in table is null

    for (j = 0; j < num_search_paths; j++)
    {
        for (k = 0; (unsigned int) k < strlen (search_path[j]); k++)
        {
            // find a slash
            c = search_path[j][k];

            if ( c == FORWARDSLASH || c == BACKSLASH)
                break;
        }

        if ( c != FORWARDSLASH && c != BACKSLASH)
            c = SLASH;


        k = strlen (search_path[j]);

        // Now does it end in a slash
        if ( search_path[j][k - 1] != c)
        {
            search_path[j][k]   = c;
            search_path[j][k + 1] = '\0';
        }
    }

    for (i = 0; i < num_search_paths; i++)
    {
        j = strlen (search_path[i]);
    }

    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleRawProgram()
{
    char Key[MAX_STRING_SIZE] = {0} , Value[MAX_STRING_SIZE] = {0};
    SIZE_T PacketLoc = 0, PacketLength, i = 0, OnKey = 1, PairFound = 0;

    // Strings in MyStringPairs will
    //    rawprogram_unsparse.xml
    //    rawprogram2.xml

    PacketLength = strlen (MyStringPairs);

    while ( IsEmptySpace ( MyStringPairs[PacketLoc] ) )
    {
        PacketLoc++;

        if (PacketLoc >= PacketLength)
            break;  // out of packet, so get out of here
    }

    // I can use this function below, as long as the newlines are replaced by commas
    // num_search_paths = SplitStringOnCommas(MyArg,search_path,num_search_paths);

    while (PacketLoc < PacketLength)
    {
        if (MyStringPairs[PacketLoc] == 0xA)
            MyStringPairs[PacketLoc] = ',';

        PacketLoc++;
    }

    if (LOAD_RAW_PROGRAM_FILES)
    {
        // MyStringPairs into XMLFileTable[MAX_XML_FILES][MAX_PATH_SIZE]
        num_xml_files_to_send = SplitStringOnCommas (MyStringPairs, sizeof (MyStringPairs), XMLFileTable, num_xml_files_to_send, MAX_XML_FILES, MAX_PATH_SIZE);

        XMLFileTable[num_xml_files_to_send][0] = '\0'; // j is how many strings were valid, make sure last one in table is null
    }

    return FIREHOSE_SUCCESS;
}

static firehose_error_t handlePatchProgram()  // used for <patch>
{
    char Key[MAX_STRING_SIZE] = {0} , Value[MAX_STRING_SIZE] = {0};
    SIZE_T PacketLoc = 0, PacketLength, i = 0, OnKey = 1, PairFound = 0;

    if (ParseAttributes == 0)
    {
        // this is like <patch>patch0.xml</patch>, i.e. when user has --xml=firehose_config.xml

        // Strings in MyStringPairs will
        //    rawprogram_unsparse.xml
        //    rawprogram2.xml

        PacketLength = strlen (MyStringPairs);

        while ( IsEmptySpace ( MyStringPairs[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= PacketLength)
                break;  // out of packet, so get out of here
        }

        // I can use this function below, as long as the newlines are replaced by commas
        // num_search_paths = SplitStringOnCommas(MyArg,search_path,num_search_paths);

        while (PacketLoc < PacketLength)
        {
            if (MyStringPairs[PacketLoc] == 0xA)
                MyStringPairs[PacketLoc] = ',';

            PacketLoc++;
        }

        if (LOAD_PATCH_PROGRAM_FILES)
        {
            // MyStringPairs into XMLFileTable[MAX_XML_FILES][MAX_PATH_SIZE]
            num_xml_files_to_send = SplitStringOnCommas (MyStringPairs, sizeof (MyStringPairs), XMLFileTable, num_xml_files_to_send, MAX_XML_FILES, MAX_PATH_SIZE);

            XMLFileTable[num_xml_files_to_send][0] = '\0'; // j is how many strings were valid, make sure last one in table is null

        }

        return FIREHOSE_SUCCESS;
    }
    else
    {
        for (i = 0; i < strlen (fh.attrs.filename); i++)
        {
            if (IsEmptySpace (fh.attrs.filename[i]) )
                fh.attrs.filename[i] = '\0';
        }

        if (strlen (fh.attrs.filename) == 0)
            return FIREHOSE_SUCCESS;

        if ( strncmp (fh.attrs.filename, "DISK", MAX (strlen (fh.attrs.filename), strlen ("DISK") ) ) != 0 )
            return FIREHOSE_SUCCESS;  // **unless it is filename="DISK", skip it

        if (!LOAD_PATCH_PROGRAM_FILES)
            return FIREHOSE_SUCCESS;  // **user doesn't want to patch

        // this is like <patch SECTOR_SIZE_IN_BYTES="512" etc
        if (fh.attrs.SECTOR_SIZE_IN_BYTES != SectorSizeInBytes)
        {
            SectorSizeInBytes = fh.attrs.SECTOR_SIZE_IN_BYTES;
            dbg (LOG_INFO, "SectorSizeInBytes changed to %d", SectorSizeInBytes);
        }

        /*
            InitBufferWithXMLHeader(tx_buffer, sizeof(tx_buffer));
            //AppendToBuffer(tx_buffer,"<data>\n");
            AppendToBuffer(tx_buffer,"<patches>\n",FIREHOSE_TX_BUFFER_SIZE);
            AppendToBuffer(tx_buffer,"<patch ",FIREHOSE_TX_BUFFER_SIZE);

            sprintf(temp_buffer,"SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ",fh.attrs.SECTOR_SIZE_IN_BYTES);
            AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

            sprintf(temp_buffer,"byte_offset=\"%"SIZE_T_FORMAT"\" ",fh.attrs.byte_offset);
            AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

            sprintf(temp_buffer,"filename=\"DISK\" ");
            AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

            sprintf(temp_buffer,"physical_partition_number=\"%"SIZE_T_FORMAT"\" ",fh.attrs.physical_partition_number);
            AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

            sprintf(temp_buffer,"size_in_bytes=\"%"SIZE_T_FORMAT"\" ",fh.attrs.size_in_bytes);
            AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

            sprintf(temp_buffer,"start_sector=\"%s\" ",fh.attrs.start_sector);
            AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

            sprintf(temp_buffer,"value=\"%s\" ",fh.attrs.value);
            AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

            AppendToBuffer(tx_buffer,"/>\n</patches>",FIREHOSE_TX_BUFFER_SIZE);
        */

        // tx_buffer already holds the XML file
        sendTransmitBuffer();


        if (Simulate)
        {
            InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
            AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
            AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
            AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
            CharsInBuffer = strlen (ReadBuffer);
        }



        GetNextPacket();  // this will set all variables, including GotACK

        if (!GotACK)
        {
            dbg (LOG_ERROR, "Something failed during patching. Please see port trace");
            ExitAndShowLog (1);
            //return FIREHOSE_ERROR;
        }


        return FIREHOSE_SUCCESS;
    }
}


static firehose_error_t handleFileMappings()
{
    // does nothing, just ignore this
    return FIREHOSE_SUCCESS;
}

void TrimTrailingWhiteSpaceFromStringVariable (char *sz)
{
    SIZE_T i = 0;

    for (i = strlen (sz); i > 0; i--)
    {
        if (sz[i] == 0x9 || sz[i] == 0x20) // tab or space
            sz[i] = '\0'; // put a null there
    }
}

static firehose_error_t handleOptions()
{
    char Key[MAX_STRING_SIZE] = {0} , Value[MAX_STRING_SIZE] = {0};
    SIZE_T PacketLoc = 0, PacketLength, i = 0, OnKey = 1, PairFound = 0;

    PacketLength = strlen (MyStringPairs);

    while (PacketLoc < PacketLength)
    {
        // Nuke any whitespace
        while ( IsEmptySpace ( MyStringPairs[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= PacketLength)
                continue;  // out of packet, so get out of here
        }

        if (OnKey)
        {
            if (i >= MAX_STRING_SIZE)
            {
                dbg (LOG_ERROR, "The key value pair in your <options> section is too big");
                ExitAndShowLog (1);
            }

            Key[i++] = MyStringPairs[PacketLoc];

            if (MyStringPairs[PacketLoc] == '=')
            {
                OnKey = 0;
                Key[i - 1] = '\0';
                i = 0;
            }
        }
        else
        {
            if (i >= MAX_STRING_SIZE)
            {
                dbg (LOG_ERROR, "The key value pair in your <options> section is too big for Value");
                ExitAndShowLog (1);
            }

            Value[i++] = MyStringPairs[PacketLoc];

            if (MyStringPairs[PacketLoc + 1] == 0xA || MyStringPairs[PacketLoc + 1] == 0xD)
            {
                OnKey = 1;
                Value[i] = '\0';
                i = 0;
                PairFound = 1;
            }
        } // end OnKey

        PacketLoc++;

        if (PairFound)
        {
            // to be here I have a Key and Value
            /*
                  if( strncmp(Key,"DOWNLOAD_PROTOCOL",MAX(strlen(Key),strlen("DOWNLOAD_PROTOCOL")))==0 )
                  {
                    // Protocol must be FIREHOSE
                    if( strncmp(Value,"FIREHOSE",strlen(Value))!=0 )
                    {
                       dbg(LOG_ERROR, "You have specified a protocol *other* than FIREHOSE. This tool only speaks FIREHOSE");
                       ExitAndShowLog(1);
                    }
                  } // end of DOWNLOAD_PROTOCOL
            */
            if ( strncmp (Key, "LOAD_RAW_PROGRAM_FILES", MAX (strlen (Key), strlen ("LOAD_RAW_PROGRAM_FILES") ) ) == 0 )
            {
                // Protocol must be FIREHOSE
                if ( strncmp (Value, "false", strlen (Value) ) == 0 )
                {
                    dbg (LOG_INFO, "You have specified to NOT load rawprogram files");
                    LOAD_RAW_PROGRAM_FILES = 0;
                }
            } // end of LOAD_RAW_PROGRAM_FILES

            if ( strncmp (Key, "LOAD_PATCH_PROGRAM_FILES", MAX (strlen (Key), strlen ("LOAD_PATCH_PROGRAM_FILES") ) ) == 0 )
            {
                // Protocol must be FIREHOSE
                if ( strncmp (Value, "false", strlen (Value) ) == 0 )
                {
                    dbg (LOG_INFO, "You have specified to NOT load patch files");
                    LOAD_PATCH_PROGRAM_FILES = 0;
                }
            } // end of LOAD_PATCH_PROGRAM_FILES

            PairFound = 0;
        } // end of if(PairFound)

    } // end of while(PacketLoc<PacketLength)



    dbg (LOG_INFO, "Parsed XML Options successfully");
    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleNop()
{
    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if (!GotACK)
        return FIREHOSE_ERROR;

    dbg (LOG_INFO, "Got the PING response");

    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleErase()
{
    dbg (LOG_INFO, "                        Issuing Erase                       ");
    dbg (LOG_INFO, " _____              _               _____                   ");
    dbg (LOG_INFO, "|_   _|            (_)             |  ___|                  ");
    dbg (LOG_INFO, "  | | ___ ___ _   _ _ _ __   __ _  | |__ _ __ __ _ ___  ___ ");
    dbg (LOG_INFO, "  | |/ __/ __| | | | | '_ \\ / _` | |  __| '__/ _` / __|/ _ \\");
    dbg (LOG_INFO, " _| |\\__ \\__ \\ |_| | | | | | (_| | | |__| | | (_| \\__ \\  __/");
    dbg (LOG_INFO, " \\___/___/___/\\__,_|_|_| |_|\\__, | \\____/_|  \\__,_|___/\\___|");
    dbg (LOG_INFO, "                             __/ |                          ");
    dbg (LOG_INFO, "                            |___/ \n\n");

    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if (!GotACK)
    {
        //return FIREHOSE_ERROR;
        dbg (LOG_ERROR,  "Something failed with the <erase> command. The port_trace.txt can shed more information on this"
             "\nNote that for eMMC <erase> is only valid for physical_partition 0. For UFS <erase> is valid for all LUNs\n\n"
            );
        ExitAndShowLog (1);
    }

    dbg (LOG_INFO, "\n\n");
    dbg (LOG_INFO, "ERASE SUCCESSFUL");
    dbg (LOG_INFO, "ERASE SUCCESSFUL");
    dbg (LOG_INFO, "ERASE SUCCESSFUL\n\n");

    return FIREHOSE_SUCCESS;
}



static firehose_error_t handleUnrecognized()
{
    /*
      SIZE_T i;

      InitBufferWithXMLHeader(tx_buffer, sizeof(tx_buffer));
        AppendToBuffer(tx_buffer,"<data>\n",FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"<%s ",NewTagName);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

      for(i=0;i<(SIZE_T)ReturnNumAttributes();i++)
      {
            if(AllAttributes[i].Assigned==0)
                continue; // variable wasn't even assigned, so no point checking

        sprintf(temp_buffer,"%s=\"%s\" ",AllAttributes[i].Name,AllAttributes[i].Raw);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);
      }

        AppendToBuffer(tx_buffer,"/>\n</data>",FIREHOSE_TX_BUFFER_SIZE);
    */

    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if(GotACK)
    {
      dbg (LOG_INFO, "Response was success in running to unrecognized TAG (%s)",
           NewTagName);
      return FIREHOSE_SUCCESS;
    }
    else
    {
      dbg (LOG_ERROR, "Error in handling to unrecognized TAG (%s)", NewTagName);
      return FIREHOSE_ERROR;
    }
}


// This function goes until a packet is found that is not a log
// so it chomps through until a <tag> is found that is not <log>, such as <response>

firehose_error_t GetNextPacket (void)
{
    int i, TimeToBreak = 0, NeedToReadFromChannel = 0;
    int NumTries = 0, MaxTries = 10000;
    SIZE_T PacketLength;

    //PacketLoc = 0;

    //dbg(LOG_DEBUG,"In GetNextPacket with CharsInBuffer = %ld",CharsInBuffer);

    BytesRead = 0; // reset here

    while (TimeToBreak == 0)
    {

        // Aaron, need to always have a /data> in the buffer, or else
        //        I need to read more to get a complete packet

        if (RawMode && !Simulate && (SIZE_T) CharsInBuffer < MaxBytesToReadFromUSB)
            NeedToReadFromChannel = 1;

        //if(PacketLoc==0)
        //  NeedToReadFromChannel = 1;
        if ( CharsInBuffer <= (XML_HEADER_LENGTH + 2 * XML_TAIL_LENGTH) )
            NeedToReadFromChannel = 1;


        if (NeedToReadFromChannel == 0 && RawMode == 0)
        {
            // make sure there is at least an end /data> in the buffer
            char *pch = NULL;

            pch = strstr ( (char *) &ReadBuffer[PacketLoc], "</data>");

            if (pch == '\0')
                NeedToReadFromChannel = 1;
            else if ( (char *) pch - (char *) &ReadBuffer[PacketLoc] < 10 && strlen (pch) <= (XML_HEADER_LENGTH + 2 * XML_TAIL_LENGTH) )
                NeedToReadFromChannel = 1;
            else
            {
                // Show what target sent
                CopyString (temp_buffer, ReadBuffer, 0, PacketLoc, pch - &ReadBuffer[PacketLoc] + strlen ("</data>"), FIREHOSE_TX_BUFFER_SIZE, MAX_READ_BUFFER_SIZE);
                //dbg(LOG_DEBUG,"\n%s", temp_buffer);
            }

        }

        //printf("\nNeedToReadFromChannel==%i",NeedToReadFromChannel);

        if (CharsInBuffer > 0 && PacketLoc > 0)
        {
            // Move the data back to the start of buffer
            for (i = 0; i < CharsInBuffer; i++)
                ReadBuffer[i] = ReadBuffer[i + PacketLoc]; // AARON - NO BOUNDS CHECK ON PacketLoc here!!!

            PacketLoc = 0; // all data moved back to the start
            memset (&ReadBuffer[CharsInBuffer], 0x0, sizeof (ReadBuffer) - CharsInBuffer);
        }


        if (NeedToReadFromChannel)
        {

            //printf("\nNeedToReadFromChannel==1");

            NeedToReadFromChannel = 0;



            if (NumTries >= MaxTries)
            {
                dbg (LOG_ERROR, "Nothing read from target for %i tries\n", MaxTries);
                ExitAndShowLog (1);
            }

            if (PacketLoc == 0)
                memset (&ReadBuffer[CharsInBuffer], 0x0, sizeof (ReadBuffer) - CharsInBuffer);

            //printf("\nAbout to call ReadPort");

            BytesRead = ReadPort ( (unsigned char*) &ReadBuffer[CharsInBuffer], MaxBytesToReadFromUSB - CharsInBuffer, MAX_READ_BUFFER_SIZE); // null doesn't matter in RAW mode

            NumTries++;

            //dbg(LOG_INFO,"Read %i bytes from port,NumTries=%d",BytesRead,NumTries);

            CharsInBuffer     += BytesRead;

            dbg (LOG_DEBUG, "CharsInBuffer = %ld", CharsInBuffer);

            //BytesReadPlusLeftOver += BytesRead;
            if (CharsInBuffer > 0)
            {
                // We have data, we can break
                //if(!RawMode || VerboseLevel==LOG_DEBUG) // User wants to see it
                if (!RawMode) // Don't show this in RAW Mode since packets can be huge! Will slow down
                    PRETTYPRINT ( (unsigned char*) ReadBuffer, CharsInBuffer, MAX_READ_BUFFER_SIZE);
                else if (PrettyPrintRawPacketsToo == 1)
                    PRETTYPRINT ( (unsigned char*) ReadBuffer, CharsInBuffer, MAX_READ_BUFFER_SIZE);

                //else
                //  dbg(LOG_DEBUG,"%d new bytes read from USB, CharsInBuffer=%d",BytesRead,CharsInBuffer);
            }
            else
            {
                // Didn't get anything, pause and try again
                dbg (LOG_INFO, "Didn't get any data from USB, sleeping for 1ms and trying again NumTries=%d of 10", NumTries);

                if (NumTries >= 10)
                    //if(NumTries>=9999999999)  // use this to debug issues with JTAG
                {
                    //break;  //Don't exit here, allow caller to handle this
                    dbg (LOG_ERROR, "Can't write to your target?");
                    ExitAndShowLog (1);
                }

                //sleep(10);    // use this to debug issues with JTAG
                sleep (1);
                dbg (LOG_INFO, "Back from sleep");
                continue; // we have no data yet
            }
        }

        NumTries        = 0;
        //dbg(LOG_INFO,"Resetting NumTries=%d",NumTries);

        if (RawMode)
            break;

        //strcpy(ReadBuffer,"<?xml version=\"1.0\" ?>\n<patches>\n<patch SECTOR_SIZE_IN_BYTES=\"512\" byte_offset=\"88\" filename=\"DISK\" physical_partition_number=\"0\" size_in_bytes=\"4\" start_sector=\"NUM_DISK_SECTORS-1.\" value=\"CRC32(NUM_DISK_SECTORS-33.,4608)\" what=\"Update Backup Header with CRC of Partition Array.\"/>\n</patches>");
        //PacketLoc = DetermineTag((char *)&ReadBuffer[PacketLoc], PacketLoc, MAX_READ_BUFFER_SIZE);    // This sets CurrentHandlerFunction()

        PacketStart = PacketLoc;
        ShowXMLFileInLog = 1;
        PacketLoc   = DetermineTag ( (char *) ReadBuffer, PacketLoc, MAX_READ_BUFFER_SIZE); // This sets CurrentHandlerFunction()
        PacketLength = ThisXMLLength;
        // PacketLoc should be past the xml and data tags, pointing at the 1st attribute
        // So if PacketLoc is still it 0, something went wrong

        if (PacketLoc == 0)
        {
            dbg (LOG_ERROR, "3. TAG not found or recognized");

            if ( (ReadBuffer[0] == 1 && ReadBuffer[4] == 48) || (ReadBuffer[0] == 4 && ReadBuffer[4] == 16) )
            {
                dbg (LOG_ERROR, "\n\n\tThere is a chance your target is in SAHARA mode!!"
                     "\n\tThere is a chance your target is in SAHARA mode!!"
                     "\n\tThere is a chance your target is in SAHARA mode!!"
                     "\n\nThis can mean"
                     "\n\t1. You forgot to send DeviceProgrammer first (i.e. QSaharaServer.exe -s 13:prog_emmc_firehose_8994_lite.mbn)"
                     "\n\t2. OR, you did send DeviceProgrammer, but it has crashed and/or is not correct for this target"
                     "\n\nRegardless this program speaks FIREHOSE protocol and your target is speaking SAHARA protcol, so this will not work\n\n");
                ExitAndShowLog (1);
            }

            return FIREHOSE_ERROR;
        }

        if (ParseAttributes)
        {
            // To be this far fh.xml_buffer[PacketLoc] is pointing to the first char of the first attribute
            PacketLoc = DetermineAttributes ( (char *) ReadBuffer, PacketLoc, MAX_READ_BUFFER_SIZE);

            if (PacketLoc == 0)
            {
                dbg (LOG_ERROR, "ATTRIBUTES not found or recognized");
                return FIREHOSE_ERROR;
            }
        }
        else
            PacketLoc = GetStringFromXML ( (char *) &ReadBuffer[PacketLoc], PacketLoc, MAX_XML_FILE_SIZE);


        // At this point some attributes will need additional processing before
        // we call the Tag Handler function
        if (ParseComplicatedAttributes() == FIREHOSE_ERROR) // i.e. start_sector="NUM_DISKSECTORS-33."
        {
            return FIREHOSE_ERROR;
        }

        // Below is the function pointer, i.e. tag_handler_t CurrentHandlerFunction;
        CurrentHandlerFunction();

        while ( IsEmptySpace ( ReadBuffer[PacketLoc] ) )
        {
            PacketLoc++;

            if ( (SIZE_T) PacketLoc >= PacketLength)
                break;
        }

        //CharsInBuffer         = BytesReadPlusLeftOver - PacketLoc; //MAX_READ_BUFFER_SIZE - PacketLoc; //BytesRead - PacketLoc;
        CharsInBuffer -= (PacketLoc - PacketStart); // (PacketLoc-PacketStart) is how many characters we used
        //BytesReadPlusLeftOver = CharsInBuffer;

        if (CharsInBuffer <= 0)
        {
            CharsInBuffer     = 0;
            PacketLoc       = 0;
            //BytesReadPlusLeftOver = 0;
        }

        //else
        //  dbg(LOG_INFO,"BUFFER: (%i bytes)\n%s",CharsInBuffer,(char *)&ReadBuffer[PacketLoc]);

        TimeToBreak = 1;  // assume in here once

        if (strncasecmp (NewTagName, "log", strlen ( (const char *) NewTagName) ) == 0)
        {
            TimeToBreak = 0;  // to be here this packet was a log, so continue
        }
    }

    //BytesRead = ReadPort(buffer, 1024);
    //PRETTYPRINT(buffer, BytesRead);
    return FIREHOSE_SUCCESS;
}

static firehose_error_t handleResponse()
{
    GotACK = 0; // reset

    dbg (LOG_DEBUG, "Response was '%s'", fh.attrs.value);

    if (strncasecmp ( (const char *) fh.attrs.value, "ack", strlen ( (const char *) fh.attrs.value) ) == 0)
    {
        GotACK = 1;
    }

    return FIREHOSE_SUCCESS;
}

void LoadResetIntoStringTable (void)
{
    InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
    AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
    AppendToBuffer (tx_buffer, "<power DelayInSeconds=\"10\" value=\"reset\" ", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, set 10 back to 2

    AppendToBuffer (tx_buffer, "/>\n</data>\n", FIREHOSE_TX_BUFFER_SIZE); // HACK for Zeno, remove the \n

    // Save into XMLFileTable
    CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
    NumXMLFilesInTable++;

    if (NumXMLFilesInTable >= MAX_XML_FILES)
    {
        dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
        ExitAndShowLog (1);
    }
}

void LoadConfigureIntoTXBuffer (void)
{
    InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
    AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
    AppendToBuffer (tx_buffer, "<configure ", FIREHOSE_TX_BUFFER_SIZE);

    sprintf (temp_buffer, "MemoryName=\"%s\" ", fh.attrs.MemoryName);
    AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    if (Verbose)
        AppendToBuffer (tx_buffer, "Verbose=\"1\" ", FIREHOSE_TX_BUFFER_SIZE);
    else
        AppendToBuffer (tx_buffer, "Verbose=\"0\" ", FIREHOSE_TX_BUFFER_SIZE);

    if (testvipimpact)
        AppendToBuffer (tx_buffer, "AlwaysValidate=\"1\" ", FIREHOSE_TX_BUFFER_SIZE);
    else
        AppendToBuffer (tx_buffer, "AlwaysValidate=\"0\" ", FIREHOSE_TX_BUFFER_SIZE);


    sprintf (temp_buffer, "MaxDigestTableSizeInBytes=\"%"SIZE_T_FORMAT"\" ", fh.attrs.MaxDigestTableSizeInBytes);
    AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    sprintf (temp_buffer, "MaxPayloadSizeToTargetInBytes=\"%"SIZE_T_FORMAT"\" ", fh.attrs.MaxPayloadSizeToTargetInBytes);
    AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

    if (fh.attrs.ZlpAwareHost)
        AppendToBuffer (tx_buffer, "ZlpAwareHost=\"1\" ", FIREHOSE_TX_BUFFER_SIZE);
    else
        AppendToBuffer (tx_buffer, "ZlpAwareHost=\"0\" ", FIREHOSE_TX_BUFFER_SIZE);

    if (fh.attrs.SkipWrite)
    {
        sprintf (temp_buffer, "SkipWrite=\"%"SIZE_T_FORMAT"\" ", fh.attrs.SkipWrite);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
    } // HACK, I removed else to match zeno

//  if(fh.attrs.SkipStorageInit)
//  {
    sprintf (temp_buffer, "SkipStorageInit=\"%"SIZE_T_FORMAT"\" ", fh.attrs.SkipStorageInit);
    AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);
//  }

    AppendToBuffer (tx_buffer, "TargetName=\"8960\" ", FIREHOSE_TX_BUFFER_SIZE); // HACK, to match Zeno

    AppendToBuffer (tx_buffer, "/>\n</data>\n", FIREHOSE_TX_BUFFER_SIZE); // HACK, the \n here is to match Zeno
}

void LoadConfigureIntoStringTable (void)
{
    // always goes at 0
    LoadConfigureIntoTXBuffer();
    // Save into XMLFileTable at location 0, not at NumXMLFilesInTable since <configure> is always our first command
    CopyString (XMLStringTable[0], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);

}
/*
void SendConfigure(void)
{
  dbg(LOG_INFO,"Sending <configure>");

  // tx_buffer already holds the XML file
    sendTransmitBuffer();

  if(Simulate)
  {
    InitBufferWithXMLHeader(&ReadBuffer[PacketLoc], sizeof(ReadBuffer)-PacketLoc);
    AppendToBuffer(ReadBuffer,"<data>\n",MAX_READ_BUFFER_SIZE);
    if(fh.attrs.MaxPayloadSizeToTargetInBytes > fh.attrs.MaxPayloadSizeToTargetInBytesSupported)
    {
      AppendToBuffer(ReadBuffer,"<response value=\"NAK\" ",MAX_READ_BUFFER_SIZE);
      sprintf(temp_buffer,"MaxPayloadSizeToTargetInBytes=\"%"SIZE_T_FORMAT"\" ",fh.attrs.MaxPayloadSizeToTargetInBytesSupported);
      AppendToBuffer(ReadBuffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);
      sprintf(temp_buffer,"MaxPayloadSizeToTargetInBytesSupported=\"%"SIZE_T_FORMAT"\" ",fh.attrs.MaxPayloadSizeToTargetInBytesSupported);
      AppendToBuffer(ReadBuffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);
    }
    else
    {
      AppendToBuffer(ReadBuffer,"<response value=\"ACK\" ",MAX_READ_BUFFER_SIZE);
      sprintf(temp_buffer,"MaxPayloadSizeToTargetInBytes=\"%"SIZE_T_FORMAT"\" ",fh.attrs.MaxPayloadSizeToTargetInBytes);
      AppendToBuffer(ReadBuffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);
      sprintf(temp_buffer,"MaxPayloadSizeToTargetInBytesSupported=\"%"SIZE_T_FORMAT"\" ",fh.attrs.MaxPayloadSizeToTargetInBytesSupported);
      AppendToBuffer(ReadBuffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);
    }
    AppendToBuffer(ReadBuffer,"/>\n</data>",MAX_READ_BUFFER_SIZE);
    CharsInBuffer = strlen(ReadBuffer);
    //sprintf(temp_buffer,"value=\"%s\" ",fh.attrs.value);
    //AppendToBuffer(ReadBuffer,temp_buffer);

  }

  GetNextPacket();  // this will set all variables, including GotACK

  // Target can ACK or NAK my configure packet. As it is setup now, all that changes is that Target will have *set* variables on PC side
  // That is fh_loader.exe will have it's MaxPayloadSizeToTargetInBytes *set* to whatever target has

  //MaxPayloadSizeToTargetInBytesSupported


  if(GotACK)
    dbg(LOG_INFO,"Got the ACK");
  else
    dbg(LOG_INFO,"Got a NAK, but this is OK. fh_loader switched to use Targets parameters!");

}
*/

void SendReset (void)
{
    dbg (LOG_INFO, "Sending RESET to target");

    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if (GotACK)
    {
        dbg (LOG_DEBUG, "Got the ACK");
    }
}

SIZE_T SplitStringOnCommas (char *sz, SIZE_T SizeOfString, char szArray[][MAX_PATH_SIZE], int offset, SIZE_T ArraySize, SIZE_T StringSize)
{
    char c;
    SIZE_T i, j, k, comma = 0;
    j = strlen (sz);

    k = offset;

    if (k >= ArraySize)
    {
        dbg (LOG_ERROR, "In SplitStringOnCommas the array cannot hold anymore strings");
        ExitAndShowLog (1);
    }

    for (i = 0; i < j; i++)
    {
        // look for comma
        c = sz[i];

        if (c == ',')
        {
            c = c;

            // NOTE: there could be white space after the comma
            while ( IsEmptySpace ( sz[comma] ) )
            {
                comma++;

                if (comma >= j)
                {
                    dbg (LOG_ERROR, "XML packet not formed correctly. Ran out of room looking for TAG");
                    return 0;
                }
            }

            if (i - comma == 0)
            {
                // user has extra enters or extra commas, i.e.
                // path1
                // path2
                //         <-- just ignore this, it will be length 0, i.e. (i-comma==0)
                // path3
            }
            else
            {
                if ( CopyString (szArray[k], sz, 0, comma, i - comma, sizeof (szArray[k]), SizeOfString)  == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy string of length %"SIZE_T_FORMAT" bytes into szArray[%"SIZE_T_FORMAT"]", i - comma, k);
                    ExitAndShowLog (1);
                }

                k++;

                if (k >= ArraySize)
                {
                    dbg (LOG_ERROR, "In SplitStringOnCommas the array cannot hold anymore strings");
                    ExitAndShowLog (1);
                }
            }

            comma = i + 1; // +1 to skip this comma
        }
    }

    if (comma < j)
    {
        if ( CopyString (szArray[k], sz, 0, comma, i - comma, sizeof (szArray[k]), SizeOfString)  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string of length %"SIZE_T_FORMAT" bytes into szArray[%"SIZE_T_FORMAT"]", i - comma, k);
            ExitAndShowLog (1);
        }

        // quick sanity check, since this is the last of the split, we could have had filename,,, where the multiple commas
        // are hard to deal with. So, lets look at szArray[k] and see if it is all spaces

        j = 0;

        while ( IsEmptySpace ( szArray[k][j] ) )
        {
            j++;

            if (j >= strlen (szArray[k]) )
                break;
        }

        if (j < strlen (szArray[k]) )
            k++;  // this means the string had something more than spaces, so let it stay


    }

    return k;

}

void CheckContentsXMLCompleteFileAndPath (char *filename)
{
    SIZE_T i = 0;

    memset (contents_full_filename_with_path, 0x0, sizeof (contents_full_filename_with_path) );

    if (NumContensXML == 0)
        return;

    dbg (LOG_DEBUG, "==================================================================================");
    dbg (LOG_DEBUG, "Seeing if file '%s' is in contents.xml", filename);


    for (i = 0; i < NumContensXML; i++)
    {
        // loop through files found in contents.xml
        if ( strncmp (filename, ContensXML[i].Filename, MAX (strlen (filename), strlen (ContensXML[i].Filename) ) ) == 0 )
        {
            // it matches
            if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0)
            {
                // to be here means either ContensXML[i].StorageType=0 which is unknown, so use it
                // OR, ContensXML[i].StorageType=='eMMC' which matches fh.attrs.MemoryName[0]

                i = i; // Aaron, send back concat path and filename

                if ( CopyString (contents_full_filename_with_path, ContensXML[i].Path, 0, 0, strlen (ContensXML[i].Path), MAX_PATH_SIZE, MAX_PATH_SIZE) == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into contents_full_filename_with_path", ContensXML[i].Filename, strlen (ContensXML[i].Filename) );
                    ExitAndShowLog (1);
                }

                if ( CopyString (contents_full_filename_with_path, ContensXML[i].Filename, strlen (ContensXML[i].Path), 0, strlen (ContensXML[i].Filename), MAX_PATH_SIZE, MAX_ATTR_NAME_SIZE) == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy string '%s' of length %i into contents_full_filename_with_path", ContensXML[i].Filename, strlen (ContensXML[i].Filename) );
                    ExitAndShowLog (1);
                }

                return;
            } // end of matching memory name
        }
    } // end i

    return;
}

void PossiblyShowContentsXMLDifferentFileFoundWarning (char *CurrentPathAndFilenameFound)
{
    if (strlen (contents_full_filename_with_path) > 0)
    {
        // means we have this file also in contents.xml that the user provided, so see if the path is the same
        // otherwise they have picked up the file in a different location then what contents.xml specified.
        // this is a FEATURE to allow users to over ride what contents.xml has. BUT, it might also be unexpected
        if ( strncasecmp (CurrentPathAndFilenameFound, contents_full_filename_with_path, MAX (strlen (CurrentPathAndFilenameFound), strlen (contents_full_filename_with_path) ) ) != 0 )
        {
            // to be here means we have found the file in a different spot then what contents.xml specified
            dbg (LOG_WARN, "You provided a contents.xml which specified '%s'\n"
                 "However searching through --search_paths found '%s'\n"
                 "This is a FEATURE to allow you to override contents.xml mappings\n"
                 "If you DID NOT WANT THIS behavior, use --forcecontentsxmlpaths\n"
                 , contents_full_filename_with_path, CurrentPathAndFilenameFound);

        }
    } // end if strlen

}

char* find_file (char *filename, char ShowToScreen)
{
    SIZE_T i = 0;
    struct _stat64 status_buf;
    FILE *fj;

    // for breakpoint only
    if ( strncasecmp (filename, "cache_1.img", MAX (strlen (filename), strlen ("cache_1.img") ) ) == 0 )
    {
        i = i;  // for breakpoint
    }

    dbg (LOG_DEBUG, "==================================================================================");
    dbg (LOG_DEBUG, "==================================================================================");

    if (ShowToScreen)
        dbg (LOG_INFO, "Looking for file '%s'", filename);
    else
        dbg (LOG_DEBUG, "Looking for file '%s'", filename);

    CheckContentsXMLCompleteFileAndPath (filename); // this will fill contents_full_filename_with_path

    if (forcecontentsxmlpaths)
    {
        // if I find this file below in another location, warn user
        // If file found above then contents_full_filename_with_path will have the path and filename
        if (strlen (contents_full_filename_with_path) > 0)
        {
            filename = contents_full_filename_with_path;
        }
    }

    // It is possible filename already has a path in it
    if (HasAPathCharacter (filename, strlen (filename) ) )
    {
        //if(IsARelativePath(filename, strlen(filename)))

        /*
            if(stat(filename, &status_buf) == 0)
            {

            }
        */

        fj = ReturnFileHandle (filename, MAX_PATH_SIZE, "rb"); // will EXIT if not successful

        LastFindFileFileSize = ReturnFileSize (fj);
        fclose (fj);
        fj = NULL;

        dbg (LOG_DEBUG, "Found '%s' (%"SIZE_T_FORMAT" bytes)", filename, LastFindFileFileSize);

        if (AlreadyHaveThisFile (filename) == 0)
        {
            if ( CopyString (MaxFilesToTrack[FileToTrackCount++], filename, 0, 0, strlen (filename), MAX_PATH_SIZE, MAX_PATH_SIZE) == 0)
            {
                dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into MaxFilesToTrack at location %"SIZE_T_FORMAT, filename, strlen (filename), FileToTrackCount);
                ExitAndShowLog (1);
            }
        }

        if ( CopyString (full_filename_with_path, filename, 0, 0, strlen (filename), sizeof (full_filename_with_path), MAX_PATH_SIZE)  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into full_filename_with_path", filename, strlen (filename) );
            ExitAndShowLog (1);
        }

        PossiblyShowContentsXMLDifferentFileFoundWarning (filename);

        return filename;
    }

//  if(stat(filename, &status_buf) == 0)
//  {
//  }

    for (i = 0; i < num_search_paths; i++)
    {
        //dbg(LOG_DEBUG, "Looking in --> %s", search_path[i]);


        if ( CopyString (full_filename_with_path, search_path[i], 0, 0, strlen (search_path[i]), sizeof (full_filename_with_path), MAX_PATH_SIZE)  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into full_filename_with_path", search_path[i], strlen (search_path[i]) );
            ExitAndShowLog (1);
        }

        //if(strncpy(full_filename_with_path, search_path[i], sizeof(full_filename_with_path)) == 0)

        if ( CopyString (full_filename_with_path, filename, strlen (full_filename_with_path), 0, strlen (filename), sizeof (full_filename_with_path), strlen (filename) )  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into full_filename_with_path", search_path[i], strlen (search_path[i]) );
            ExitAndShowLog (1);
        }

        //if (strncat(full_filename_with_path, filename, sizeof(full_filename_with_path)) == 0)

        dbg (LOG_DEBUG, "1. Calling stat(%s')", full_filename_with_path);

        if (_stat64 (full_filename_with_path, &status_buf) == 0)
        {
            fj = ReturnFileHandle (full_filename_with_path, MAX_PATH_SIZE, "rb"); // will exit if not successful

            LastFindFileFileSize = ReturnFileSize (fj);
            fclose (fj);
            fj = NULL;

            dbg (LOG_DEBUG, "Found '%s' (%"SIZE_T_FORMAT" bytes)", full_filename_with_path, LastFindFileFileSize);

            if (AlreadyHaveThisFile (full_filename_with_path) == 0)
                CopyString (MaxFilesToTrack[FileToTrackCount++], full_filename_with_path, 0, 0, strlen (full_filename_with_path), MAX_PATH_SIZE, MAX_PATH_SIZE);

            PossiblyShowContentsXMLDifferentFileFoundWarning (full_filename_with_path);

            return full_filename_with_path;
        }
    }

    dbg (LOG_DEBUG, "2. Calling stat(%s')", filename);

    if (_stat64 (filename, &status_buf) == 0)
    {
        CopyString (full_filename_with_path, cwd, 0, 0, strlen (cwd), MAX_PATH_SIZE, MAX_PATH_SIZE);
        CopyString (full_filename_with_path, filename, strlen (cwd), 0, strlen (filename), MAX_PATH_SIZE, strlen (filename) );

        //fj = ReturnFileHandle(filename, MAX_PATH_SIZE, "rb"); // will exit if not successful
        fj = ReturnFileHandle (full_filename_with_path, MAX_PATH_SIZE, "rb"); // will exit if not successful
        LastFindFileFileSize = ReturnFileSize (fj);
        fclose (fj);
        fj = NULL;

        dbg (LOG_DEBUG, "Found '%s' (%"SIZE_T_FORMAT" bytes) in local directory '%s", filename, LastFindFileFileSize, cwd);

        //CopyString(MaxFilesToTrack[FileToTrackCount-1], filename, strlen(cwd), 0, strlen(filename), MAX_PATH_SIZE, strlen(filename));
        PossiblyShowContentsXMLDifferentFileFoundWarning (full_filename_with_path);

        return full_filename_with_path;
    }

    // to be here means we didn't find the file yet, so possibly we can use the contents.xml mapping if it was provided
    if (strlen (contents_full_filename_with_path) > 0)
    {
        dbg (LOG_INFO, "Using contents.xml mapping '%s'", contents_full_filename_with_path);
        return contents_full_filename_with_path;
    }

    dbg (LOG_WARN, "Couldn't find the file '%s', returning NULL", filename);
    return NULL;
}

int ThisFileIsInFilterFiles (char *filename_only)
{
    SIZE_T i;

    for (i = 0; i < num_filter_files; i++)
    {
        if ( strncasecmp (filter_files[i], filename_only, MAX (strlen (filter_files[i]), strlen (filename_only) ) ) == 0 )
        {
            return 1; // TRUE, load this file
        }
    }

    // This file is an exception Zeros16KB.bin
    if ( strncasecmp ("Zeros16KB.bin", filename_only, MAX (strlen ("Zeros16KB.bin"), strlen (filename_only) ) ) == 0 )
    {
        return 1; // TRUE, load this file
    }

    return 0; // don't load this file
}

int ThisFileIsInNotFilterFiles (char *filename_only)
{
    SIZE_T i;

    for (i = 0; i < num_filter_not_files; i++)
    {
        if ( strncasecmp (filter_not_files[i], filename_only, MAX (strlen (filter_not_files[i]), strlen (filename_only) ) ) == 0 )
        {
            return 1; // TRUE, do not load this file
        }
    }

    return 0; // FALSE, user is ok to load this file
}

int AlreadyHaveThisFile (char *full_filename_with_path)
{
    SIZE_T i;

    for (i = 0; i < FileToTrackCount; i++)
    {
        if ( strncasecmp (MaxFilesToTrack[i], full_filename_with_path, MAX (strlen (MaxFilesToTrack[i]), strlen (full_filename_with_path) ) ) == 0 )
        {
            return 1; // TRUE, we already have this file
        }
    }

    return 0; // FALSE, we do not have this file yet
}

int AlreadyHaveThisPath (char *path)
{
    SIZE_T i;

    for (i = 0; i < num_search_paths; i++)
    {
        if ( strncmp (search_path[i], path, MAX (strlen (search_path[i]), strlen (path) ) ) == 0 )
        {
            return 1; // TRUE, we already have this file
        }
    }

    return 0; // FALSE, we do not have this file yet
}


void OpenAndStoreAllXMLFiles (void)
{
    SIZE_T k = 0;
    char *XmlFile = NULL;

    while (XMLFileTable[k][0] != '\0')
    {
        dbg (LOG_INFO, "\n\n\n");
        dbg (LOG_INFO, "Trying to store '%s' in string table", XMLFileTable[k]);
        XmlFile = find_file (XMLFileTable[k], 1);

        if (XmlFile == NULL)
        {
            dbg (LOG_ERROR, "'%s' not found", XMLFileTable[k]);
            ExitAndShowLog (1);
        }

        // To be here we have a file to parse
        StoreXMLFile (XmlFile); // modifies EntireXMLFileBuffer
        XMLFileTable[k][0] = '\0';
        k++;
    }
}

void SendXmlFiles (void)
{
    SIZE_T k = 0;

    while (XMLStringTable[k][0] != '\0')
    {
        if (SimulateForFileSize)
        {
            // only care about sending <program when SimulateForFileSize
            char *pch, *pch2;
            pch = strstr (XMLStringTable[k], "<program");
            pch2 = strstr (XMLStringTable[k], "<read");

            if (pch == NULL && pch2 == NULL)
            {
                k++;  // move to next file
                continue;
            }
        }

        //dbg(LOG_ALWAYS,"FOR DEBUG XMLStringTable[%i] is\n%s",k,XMLStringTable[k]);
        SendXMLString (XMLStringTable[k], MAX_XML_SIZE);
        /*
            if(stresstest)
            {
              // this file will grow crazy large doing this
              fclose(fp);
              fp = ReturnFileHandle(PortTraceName, MAX_PATH_SIZE, "w");     // will exit if not successful
            }
        */
        k++;
    }

    NumXMLFilesInTable = k;
}

void StoreXMLFile (char *FileWithPath)
{
    SIZE_T PacketLoc   = 0, PacketLocBack = 0;
    SIZE_T XMLFileSize = 0;
    SIZE_T FileSizeNumSectors = 0;
    char *pch;
    char single_xml_buffer[MAX_STRING_SIZE];
    char *sparse_pch = NULL;
    char *ffu_pch = NULL;
    int i;
    char *FileToSendWithPath = NULL;
    struct sparse_handle_t * sparse_file_handle = NULL;
    struct FFUImage_t *FFU = NULL;

    FILE *fd = ReturnFileHandle (FileWithPath, MAX_PATH_SIZE, "r" ); // will exit if not successful

    XMLFileSize = ReturnFileSize (fd);

    if (XMLFileSize > sizeof (EntireXMLFileBuffer))
    {
        dbg (LOG_ERROR, "'%s' of size %"SIZE_T_FORMAT" is bigger than %lld buffer size. Please reduce the file size.", FileWithPath, XMLFileSize, sizeof (EntireXMLFileBuffer));
        ExitAndShowLog (1);
    }

    memset (EntireXMLFileBuffer, 0, sizeof (EntireXMLFileBuffer) );
    fread (EntireXMLFileBuffer, XMLFileSize, 1, fd);
    fclose (fd);
    fd = NULL;

    /*
      // I can handle only a single </ in the file, which should come at the end, i.e. </configuration> or </data>
      // so if I find more than one, probably somebody has a compound XML file, so I'll just ignore it
      pch = EntireXMLFileBuffer;
      pch = strstr(pch, "</");
      if(pch!=NULL)
      {
        pch++;  // move 1 further
        pch = strstr(pch, "</");
        if(pch!=NULL)
        {
          dbg(LOG_INFO,"The XML file has compound xml tags that cannot be handled. Ignoring '%s'",FileWithPath);
          return;
        }
      }
    */

    // Could be a config.xml file created by parseContentsXML.py
    pch = strstr (EntireXMLFileBuffer, "<options>");

    if (pch != NULL)
    {
        pch = strstr (EntireXMLFileBuffer, "<search_paths>");

        if (pch != NULL)
        {
            parseConfigXML (XMLFileSize); // works on EntireXMLFileBuffer
            CleanseSearchPaths();
            return;
        }
    }

    // Could be a partition_nand.xml, which we don't support
    pch = strstr (EntireXMLFileBuffer, "<magic_numbers>");

    if (pch != NULL)
    {
        dbg (LOG_WARN, "Found 'magic_numbers' tag, appears to be a partition_nand.xml, so ignoring this");
        return;
    }

    while (1)
    {
        // EntireXMLFileBuffer holds the entire XML file, which might have multiple <tags>
        ShowXMLFileInLog = 0;
        PacketLoc = DetermineTag ( (char *) EntireXMLFileBuffer, PacketLoc, MAX_XML_FILE_SIZE); // This sets CurrentHandlerFunction()


        // Note, if I find <parser_instructions>, then this is an older contents.xml where they include partition.xml
        //       so just bail, I only care about rawprogram.xml and patch.xml

        if ( strncmp (NewTagName, "parser_instructions", MAX (strlen (NewTagName), strlen ("parser_instructions") ) ) == 0 )
        {
            dbg (LOG_DEBUG, "File '%s' is being ignored since it contained <parser_instructions> and is probably a partition.xml file", FileWithPath);
            return;
        }

        // This chomps up empty space till we hit a tag
        while ( IsEmptySpace ( EntireXMLFileBuffer[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= XMLFileSize)
            {
                break;  // we have run out of packet all is well
            }
        }

        //XMLFileSize = strlen((char *)EntireXMLFileBuffer); // since DetermineTag() can replace final '\n' with '\0'

        if (PacketLoc == 0)
        {
            dbg (LOG_ERROR, "1. TAG not found or recognized");
            return; // FIREHOSE_ERROR;
        }

        pch = strstr (EntireXMLFileBuffer, "/>");

        if (pch == NULL)
        {
            dbg (LOG_ERROR, "XML is not formatted correctly. Could not find closing />\n\n");
            ExitAndShowLog (1);
        }

        // NOTE: Below I parse attributes *only* to support the wipefirst feature. This is because I need to know the
        //       start sector at which to write 16KB of zeros. However, ParseAttributes will *increase* PacketLoc
        //       which I don't want, so back it up, and replace when done.

        PacketLocBack = PacketLoc;

        if (ParseAttributes)
        {
            // To be this far fh.xml_buffer[PacketLoc] is pointing to the first char of the first attribute
            PacketLoc = DetermineAttributes ( (char *) EntireXMLFileBuffer, PacketLoc, MAX_XML_FILE_SIZE);

            if (PacketLoc == 0)
            {
                dbg (LOG_ERROR, "ATTRIBUTES not found or recognized");
                return; // FIREHOSE_ERROR;
            }
        }

        PacketLoc = PacketLocBack;

        if ( strncmp (NewTagName, "program", MAX (strlen (NewTagName), strlen ("program") ) ) == 0 )
        {
            // To be here means we're looking at a <program command. This is the only tag I want
            // to possibly add an <erase or <program filename="Zeros16KB.bin"

            if (wipefirst)
            {
                InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
                AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
                AppendToBuffer (tx_buffer, "<program ", FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SectorSizeInBytes);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", ( (16 * 1024) / SectorSizeInBytes) );
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "filename=\"%s\" ", WipeFirstFileName);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", fh.attrs.physical_partition_number);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "start_sector=\"%s\" ", fh.attrs.start_sector);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);


                // Save into XMLFileTable
                CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
                NumXMLFilesInTable++;

                if (NumXMLFilesInTable >= MAX_XML_FILES)
                {
                    dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                    ExitAndShowLog (1);
                }
            }

            if (erasefirst)
            {
                InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
                AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
                AppendToBuffer (tx_buffer, "<erase ", FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", SectorSizeInBytes);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", fh.attrs.num_partition_sectors );
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", fh.attrs.physical_partition_number);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                sprintf (temp_buffer, "start_sector=\"%s\" ", fh.attrs.start_sector);
                AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

                AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);


                // Save into XMLFileTable
                CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
                NumXMLFilesInTable++;

                if (NumXMLFilesInTable >= MAX_XML_FILES)
                {
                    dbg (LOG_ERROR, "1. Too many XML files in XMLStringTable, max is %d", XMLStringTable);
                    ExitAndShowLog (1);
                }
            } // end of erasefirst

        } // end of this is a <program command

        memcpy (single_xml_buffer, EntireXMLFileBuffer + PacketLoc, pch - &EntireXMLFileBuffer[PacketLoc]);
        sparse_pch = strstr (single_xml_buffer, "sparse=\"true\"");
        ffu_pch = strstr(single_xml_buffer, "windows_ffu=\"true\"");
        
        if (sparse_pch != NULL && (strcmp (NewTagName, "program") == 0) && fh.attrs.filename[0] != '\0')
        {
            //ITS A SPARSE IMAGE!!!
            //fh.attrs.filename is correct at this point...
            for (i = 0; i < strlen (fh.attrs.filename); i++)
            {
                if (IsEmptySpace (fh.attrs.filename[i]) )
                    fh.attrs.filename[i] = '\0';
            }

            FileToSendWithPath = find_file (fh.attrs.filename, 1);

            if (FileToSendWithPath != NULL)
            {
                fd = ReturnFileHandle(FileToSendWithPath, MAX_PATH_SIZE, "rb"); // will exit if not successful
                GenerateSparseXMLTags(fd, fh.attrs.filename, atoi(fh.attrs.start_sector), fh.attrs.SECTOR_SIZE_IN_BYTES, fh.attrs.physical_partition_number);
            }
        }
        else if (ffu_pch != NULL && (strcmp(NewTagName, "program") == 0) && fh.attrs.filename[0] != '\0')
        {
            for (i = 0; i < strlen(fh.attrs.filename); i++)
            {
              if (IsEmptySpace(fh.attrs.filename[i]))
                  fh.attrs.filename[i] = '\0';
            }
            FileToSendWithPath = find_file(fh.attrs.filename, 1);
            if (FileToSendWithPath != NULL)
            {
                fd = ReturnFileHandle(FileToSendWithPath, MAX_PATH_SIZE, "rb"); // will exit if not successful
                FFU = InitializeFFU(fh.attrs.filename, fh.attrs.SECTOR_SIZE_IN_BYTES, fh.attrs.physical_partition_number);
                ParseFFUHeaders(FFU, fd);
                ReadFFUGPT(FFU, fd);
                DumpFFURawProgram(FFU);
                CloseFFU(FFU);
            }
            
        }
        else
        {
            InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
            AppendToBuffer (tx_buffer, "<data>\n<", FIREHOSE_TX_BUFFER_SIZE);

            sprintf (temp_buffer, "%s ", NewTagName);
            AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

            CopyString (tx_buffer, EntireXMLFileBuffer, strlen (tx_buffer), PacketLoc, pch - &EntireXMLFileBuffer[PacketLoc], FIREHOSE_TX_BUFFER_SIZE, MAX_XML_FILE_SIZE);
            AppendToBuffer (tx_buffer, " />\n</data>", FIREHOSE_TX_BUFFER_SIZE);


            CopyString (XMLStringTable[NumXMLFilesInTable], tx_buffer, 0, 0, strlen (tx_buffer), MAX_XML_SIZE, FIREHOSE_TX_BUFFER_SIZE);
            NumXMLFilesInTable++;
        }

        PacketLoc += pch - &EntireXMLFileBuffer[PacketLoc] + 2; // +2 gets past the />

        if (NumXMLFilesInTable >= MAX_XML_FILES)
        {
            dbg (LOG_ERROR, "2. Too many XML files in XMLStringTable, max is %d", MAX_XML_FILES);
            ExitAndShowLog (1);
        }


        // This chomps up empty space till we hit a tag
        while ( IsEmptySpace ( EntireXMLFileBuffer[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= XMLFileSize)
            {
                break;  // we have run out of packet all is well
            }
        }

        if (EntireXMLFileBuffer[PacketLoc] == 0)
            break;  // we're out of packet, all is well

        //if(strlen( &EntireXMLFileBuffer[PacketLoc] ) == 0)
        //  break;

    }
}

/*
void SendXMLFile(char *FileWithPath)
{
  SIZE_T PacketLoc   = 0;
  SIZE_T XMLFileSize = 0;

  FILE *fd = fopen(FileWithPath, "r");
  if(fd==NULL)
  {
        dbg(LOG_ERROR, "Could not open '%s'",FileWithPath);
    ExitAndShowLog(1);
  }

  XMLFileSize = ReturnFileSize(fd);

  if(XMLFileSize > MAX_XML_FILE_SIZE)
  {
        dbg(LOG_ERROR, "'%s' of size %"SIZE_T_FORMAT" is bigger than %"SIZE_T_FORMAT" buffer size. Please reduce the file size",FileWithPath,XMLFileSize,MAX_XML_FILE_SIZE);
    ExitAndShowLog(1);
  }

  memset(EntireXMLFileBuffer,0,sizeof(EntireXMLFileBuffer));
  fread(EntireXMLFileBuffer, XMLFileSize, 1, fd);

  while(1)
  {

    //PacketLoc = DetermineTag((char *)&EntireXMLFileBuffer[PacketLoc], PacketLoc, MAX_XML_FILE_SIZE);    // This sets CurrentHandlerFunction()
    PacketLoc = DetermineTag((char *)EntireXMLFileBuffer, PacketLoc, MAX_XML_FILE_SIZE);    // This sets CurrentHandlerFunction()

    if(PacketLoc==0)
    {
      dbg(LOG_ERROR,"1. TAG not found or recognized");
      return; // FIREHOSE_ERROR;
    }

    if(ParseAttributes)
    {
      // To be this far fh.xml_buffer[PacketLoc] is pointing to the first char of the first attribute
      PacketLoc = DetermineAttributes((char *)EntireXMLFileBuffer, PacketLoc, MAX_XML_FILE_SIZE);

      if(PacketLoc==0)
      {
        dbg(LOG_ERROR,"ATTRIBUTES not found or recognized");
        return; // FIREHOSE_ERROR;
      }
    }
    else
    {
      PacketLoc = GetStringFromXML((char *)&EntireXMLFileBuffer[PacketLoc], PacketLoc, MAX_XML_FILE_SIZE);
    }


        // At this point some attributes will need additional processing before
        // we call the Tag Handler function
        if(ParseComplicatedAttributes()==FIREHOSE_ERROR)    // i.e. start_sector="NUM_DISKSECTORS-33."
        {  return;        }

    // Below is the function pointer, i.e. tag_handler_t CurrentHandlerFunction;
    CurrentHandlerFunction();

    if(XMLFileSize-PacketLoc<50)
      break;
  }


  fclose(fd);

  // EntireXMLFileBuffer holds the entire XML file

}
*/

void SendXMLString (char *sz, SIZE_T MaxLength)
{
    SIZE_T PacketLoc   = 0;
    SIZE_T XMLFileSize = strlen (sz);

    if (XMLFileSize > MaxLength)
    {
        dbg (LOG_ERROR, "The XML string of size %"SIZE_T_FORMAT" is bigger than %"SIZE_T_FORMAT" buffer size. Please reduce the file size", XMLFileSize, MaxLength);
        ExitAndShowLog (1);
    }

    // Load tx_buffer with XML to send
    CopyString (tx_buffer, sz, 0, 0, XMLFileSize, FIREHOSE_TX_BUFFER_SIZE, MaxLength);

    while (1)
    {

        //PacketLoc = DetermineTag((char *)&EntireXMLFileBuffer[PacketLoc], PacketLoc, MAX_XML_FILE_SIZE);    // This sets CurrentHandlerFunction()
        ShowXMLFileInLog = 0;
        PacketLoc = DetermineTag ( (char *) sz, PacketLoc, MaxLength); // This sets CurrentHandlerFunction()

        // This chomps up empty space till we hit a tag
        while ( IsEmptySpace ( sz[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= XMLFileSize)
            {
                break;  // we have run out of packet all is well
            }
        }

        //XMLFileSize = strlen((char *)sz); // since DetermineTag() can replace final '\n' with '\0'

        if (PacketLoc == 0)
        {
            dbg (LOG_ERROR, "1. TAG not found or recognized");
            return; // FIREHOSE_ERROR;
        }

        if (ParseAttributes)
        {
            // To be this far fh.xml_buffer[PacketLoc] is pointing to the first char of the first attribute
            PacketLoc = DetermineAttributes ( (char *) sz, PacketLoc, MaxLength);

            if (PacketLoc == 0)
            {
                dbg (LOG_ERROR, "ATTRIBUTES not found or recognized");
                return; // FIREHOSE_ERROR;
            }
        }
        else
        {
            PacketLoc = GetStringFromXML ( (char *) &sz[PacketLoc], PacketLoc, MaxLength);
        }


        // At this point some attributes will need additional processing before
        // we call the Tag Handler function
        if (ParseComplicatedAttributes() == FIREHOSE_ERROR) // i.e. start_sector="NUM_DISKSECTORS-33."
        {
            return;
        }

        // Below is the function pointer, i.e. tag_handler_t CurrentHandlerFunction;
        CurrentHandlerFunction();

        // This chomps up empty space till we hit a tag
        while ( IsEmptySpace ( sz[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= XMLFileSize)
            {
                break;  // we have run out of packet all is well
            }
        }

        if (sz[PacketLoc] == 0)
            break;  // we're out of packet, all is well

        //if(PacketLoc>=XMLFileSize)
        //  break;
    }

    // Put tx_buffer back into sz, which preserves it for additional runs
    CopyString (sz, tx_buffer, 0, 0, XMLFileSize, FIREHOSE_TX_BUFFER_SIZE, MaxLength);

}


void display_error (FILE *MyFP)
{
    if (MyFP != NULL)
    {
        fprintf (MyFP, "\n\n\n");
        fprintf (MyFP, "\t _____                    \n");
        fprintf (MyFP, "\t|  ___|                   \n");
        fprintf (MyFP, "\t| |__ _ __ _ __ ___  _ __ \n");
        fprintf (MyFP, "\t|  __| '__| '__/ _ \\| '__|\n");
        fprintf (MyFP, "\t| |__| |  | | | (_) | |   \n");
        fprintf (MyFP, "\t\\____/_|  |_|  \\___/|_|  \n\n");
    }

    printf ("\n\n\n");
    printf ("\t _____                    \n");
    printf ("\t|  ___|                   \n");
    printf ("\t| |__ _ __ _ __ ___  _ __ \n");
    printf ("\t|  __| '__| '__/ _ \\| '__|\n");
    printf ("\t| |__| |  | | | (_) | |   \n");
    printf ("\t\\____/_|  |_|  \\___/|_|  \n\n");

}

void display_warning (FILE *MyFP)
{
    if (MyFP != NULL)
    {
        fprintf (MyFP, "\n\n\n");
        fprintf (MyFP, "\t                         (_)            \n");
        fprintf (MyFP, "\t__      ____ _ _ __ _ __  _ _ __   __ _ \n");
        fprintf (MyFP, "\t\\ \\ /\\ / / _` | '__| '_ \\| | '_ \\ / _` |\n");
        fprintf (MyFP, "\t \\ V  V / (_| | |  | | | | | | | | (_| |\n");
        fprintf (MyFP, "\t  \\_/\\_/ \\__,_|_|  |_| |_|_|_| |_|\\__, |\n");
        fprintf (MyFP, "\t                                   __/ |\n");
        fprintf (MyFP, "\t                                  |___/ \n\n");
    }

    printf ("\n\n\n");
    printf ("\t                         (_)            \n");
    printf ("\t__      ____ _ _ __ _ __  _ _ __   __ _ \n");
    printf ("\t\\ \\ /\\ / / _` | '__| '_ \\| | '_ \\ / _` |\n");
    printf ("\t \\ V  V / (_| | |  | | | | | | | | (_| |\n");
    printf ("\t  \\_/\\_/ \\__,_|_|  |_| |_|_|_| |_|\\__, |\n");
    printf ("\t                                   __/ |\n");
    printf ("\t                                  |___/ \n\n");

}

FILE * ReturnFileHandle (char *filename, long MaxLength, char *AccessMode)
{
    char FilePlusOutputDir[MAX_PATH_SIZE];
    FILE *FTemp;
    char *pch;
    char *w_pch = NULL;
    char *dir_pch = NULL;
    char *dir2_pch = NULL;
    //int ret;
    w_pch = strchr(AccessMode, 'w'); //check if "w" is in AccessMode
    dir_pch = strchr(filename, '\\'); //assume that with '\', user has a particular directory for the file in mind
    dir2_pch = strchr(filename, '/'); //assume that with '/', user has a particular directory for the file in mind
    
    if ((strlen (MainOutputDir) > 0) && (w_pch != NULL) && (dir_pch == NULL) &&(dir2_pch == NULL)) // user specified this at command line
    {
        CopyString (FilePlusOutputDir, MainOutputDir, 0, 0, strlen (MainOutputDir), MAX_PATH_SIZE, MAX_PATH_SIZE);
        strcat(FilePlusOutputDir, "/");
        CopyString (FilePlusOutputDir, filename, strlen (FilePlusOutputDir), 0, strlen (filename), MAX_PATH_SIZE, MaxLength);

        dbg (LOG_INFO, "1. Calling fopen('%s') with AccessMode='%s'", FilePlusOutputDir, AccessMode);
        FTemp = fopen (FilePlusOutputDir, AccessMode);

        if (FTemp != NULL)
        {
            //dbg(LOG_DEBUG,"Successfully called fopen with access mode '%s' on '%s'",AccessMode,FilePlusOutputDir);

            if (AlreadyHaveThisFile (FilePlusOutputDir) == 0)
                CopyString (MaxFilesToTrack[FileToTrackCount++], FilePlusOutputDir, 0, 0, strlen (FilePlusOutputDir), MAX_PATH_SIZE, MAX_PATH_SIZE);

            return FTemp;
        }
    }

    // They may have provided a path already
    dbg (LOG_DEBUG, "2. Calling fopen('%s') with AccessMode='%s'", filename, AccessMode);
    FTemp = fopen (filename, AccessMode);

    if (FTemp != NULL)
    {
        // if there is no slashes in the filename, they did not provide a patch
        pch = strstr (filename, "/");

        if (pch == NULL)
            pch = strstr (filename, "\\");

        if (pch == NULL)
        {
            // they didn't provide a path
            CopyString (FilePlusOutputDir, cwd, 0, 0, strlen (cwd), MAX_PATH_SIZE, MAX_PATH_SIZE);
            CopyString (FilePlusOutputDir, filename, strlen (FilePlusOutputDir), 0, strlen (filename), MAX_PATH_SIZE, MaxLength);
        }
        else
            CopyString (FilePlusOutputDir, filename, 0, 0, strlen (filename), MAX_PATH_SIZE, MaxLength);

        if (AlreadyHaveThisFile (FilePlusOutputDir) == 0)
            CopyString (MaxFilesToTrack[FileToTrackCount++], FilePlusOutputDir, 0, 0, strlen (FilePlusOutputDir), MAX_PATH_SIZE, MAX_PATH_SIZE);


        //dbg(LOG_DEBUG,"Successfully called fopen with access mode '%s' on '%s'",AccessMode,FilePlusOutputDir);

        return FTemp;
    }

    // It is possible filename already has a path in it
    if (HasAPathCharacter (filename, strlen (filename) ) )
    {
        dbg (LOG_ERROR, "Could not access '%s' with acess mode '%s'", filename, AccessMode);
        ExitAndShowLog (1);
    }

    // To be this far the above MainOutputDir wasn't provided and/or it didn't work
    // so let's try the CWD

    CopyString (FilePlusOutputDir, cwd, 0, 0, strlen (cwd), MAX_PATH_SIZE, MAX_PATH_SIZE);
    CopyString (FilePlusOutputDir, filename, strlen (FilePlusOutputDir), 0, strlen (filename), MAX_PATH_SIZE, MaxLength);

    dbg (LOG_INFO, "3. Calling fopen('%s') with AccessMode='%s'", FilePlusOutputDir, AccessMode);
    FTemp = fopen (FilePlusOutputDir, AccessMode);

    if (FTemp != NULL)
    {
        //dbg(LOG_DEBUG,"Successfully called fopen with access mode '%s' on '%s'",AccessMode,FilePlusOutputDir);

        if (AlreadyHaveThisFile (FilePlusOutputDir) == 0)
            CopyString (MaxFilesToTrack[FileToTrackCount++], FilePlusOutputDir, 0, 0, strlen (FilePlusOutputDir), MAX_PATH_SIZE, MAX_PATH_SIZE);

        return FTemp;
    }

    dbg (LOG_ERROR, "Calling fopen with with access mode '%s' on '%s was NOT successful' ", AccessMode, FilePlusOutputDir);
    ExitAndShowLog (1);

    return NULL;
}

static firehose_error_t handleFirmwareWrite()
{
    firehose_error_t ReturnValue;
    ConvertProgram2Firmware = 1;
    dbg (LOG_INFO, "In handleFirmwareWrite, passing control to handleProgram()");
    ReturnValue = handleProgram();

    if (ReturnValue == FIREHOSE_SUCCESS)
    {
        dbg (LOG_INFO, "\n");
        dbg (LOG_INFO, "                      (firmware updated)");
        dbg (LOG_INFO, "______ ______________  ____    _  ___  ______ _____   _   _____________  ___ _____ ___________ ");
        dbg (LOG_INFO, "|  ___|_   _| ___ \\  \\/  | |  | |/ _ \\ | ___ \\  ___| | | | | ___ \\  _  \\/ _ \\_   _|  ___|  _  \\");
        dbg (LOG_INFO, "| |_    | | | |_/ / .  . | |  | / /_\\ \\| |_/ / |__   | | | | |_/ / | | / /_\\ \\| | | |__ | | | |");
        dbg (LOG_INFO, "|  _|   | | |    /| |\\/| | |/\\| |  _  ||    /|  __|  | | | |  __/| | | |  _  || | |  __|| | | |");
        dbg (LOG_INFO, "| |    _| |_| |\\ \\| |  | \\  /\\  / | | || |\\ \\| |___  | |_| | |   | |/ /| | | || | | |___| |/ / ");
        dbg (LOG_INFO, "\\_|    \\___/\\_| \\_\\_|  |_/\\/  \\/\\_| |_/\\_| \\_\\____/   \\___/\\_|   |___/ \\_| |_/\\_/ \\____/|___/\n\n");
    }

    return ReturnValue;
}
static firehose_error_t handleProgram()
{
    SIZE_T FileSize = 0, FileSizeNumSectors = 0, BytesRead = 0, NumSectors = 0, i, TotalSectorsFlashed = 0;
    char *FileToSendWithPath = NULL;
    //char FileToReadBackWithPath[MAX_PATH_SIZE];
    char MyTempString[1024] = {0};
    FILE *fd;
    struct timeval time_start, time_end, AbsoluteTimeStart;
    struct timeval network_start;
    double TimeInSeconds = 0.0, NetworkElapsed = 0.0;
    char *pch;
    SIZE_T TempBufferSize = 0, TempLength = 0;
    char *OperationString[] = {"program", "firmwarewrite", "read"};
    char Index = 0; // defaults to program

    boolean isSparseFile = FALSE;
    char temp_num[20];
    struct sparse_read_info_t sparse_read_handle;
    struct sparse_header_t sparse_header;
    int result;

    if (ConvertProgram2Firmware)
    {
        Index = 1;
    }

    for (i = 0; i < strlen (fh.attrs.filename); i++)
    {
        if (IsEmptySpace (fh.attrs.filename[i]) )
            fh.attrs.filename[i] = '\0';
    }

    if (strlen (fh.attrs.filename) == 0)
        return FIREHOSE_SUCCESS;


    if (num_filter_files > 0)
    {
        if (!ThisFileIsInFilterFiles (fh.attrs.filename) )
        {
            dbg (LOG_DEBUG, "'%s' is being SKIPPED since it was not in --files provided by the user at the command line", fh.attrs.filename);
            return FIREHOSE_SUCCESS;
        }
    }

    if (num_filter_not_files > 0)
    {
        if (ThisFileIsInNotFilterFiles (fh.attrs.filename) )
        {
            dbg (LOG_INFO, "handleProgram() '%s' is being SKIPPED since it was in --notfiles provided by the user at the command line", fh.attrs.filename);
            return FIREHOSE_SUCCESS;
        }
    }

    if (createconfigxml || SimulateForFileSize || verify_build)
    {
        // don't need to print anything, it's just messy
    }
    else
        dbg (LOG_INFO, "In handleProgram('%s')", fh.attrs.filename); // so user knows what were doing

    pch = strstr (tx_buffer, "sparse=\"true\"");

    if (pch != NULL)
    {
        isSparseFile = TRUE;
        dbg (LOG_INFO, "%s is a sparse image", fh.attrs.filename);
    }

    // to be this far we are in RAW MODE. Need to transfer the file
    FileToSendWithPath = find_file (fh.attrs.filename, 1);

    if (FileToSendWithPath == NULL)
    {
        // File doesn't exist, BUT, if we are flattening a build, just WARN the user, don't bail
        if (FlattenBuild || verify_build)
        {
            dbg (LOG_WARN, "'%s' not found. You will need --notfiles=%s when loading this build\n\n", fh.attrs.filename, fh.attrs.filename);
            return FIREHOSE_SUCCESS;
        }
        else
        {
            if (!PromptUser)
            {
                dbg (LOG_ERROR, "'%s' not found. You could possibly try --notfiles=%s,OtherFileToSkip.bin (note, exiting since you specified --noprompt)", fh.attrs.filename, fh.attrs.filename);
                ExitAndShowLog (1);
            }
            else
            {
                dbg (LOG_WARN, "'%s' not found. You will need --notfiles=%s when loading this build\n"
                     "OR, you need to provide a --search_path to this file\n\n", fh.attrs.filename, fh.attrs.filename);
                return FIREHOSE_SUCCESS;
            }
        }
    }

    if (fh.attrs.SECTOR_SIZE_IN_BYTES != SectorSizeInBytes)
    {
        SectorSizeInBytes = fh.attrs.SECTOR_SIZE_IN_BYTES;
        dbg (LOG_INFO, "SectorSizeInBytes changed to %d", SectorSizeInBytes);
    }

    fd = ReturnFileHandle (FileToSendWithPath, MAX_PATH_SIZE, "rb"); // will exit if not successful

    if (isSparseFile)
    {
        boolean num_conversion;
        pch = strstr (tx_buffer, "UNSPARSE_FILE_SIZE=\""); //go to unsparse file size
        pch = strstr (&tx_buffer[pch - tx_buffer], "\""); //find terminating area
        pch += 1; //gets rid of the first "
        //get the actual numbers from file size
        i = 0;

        while (pch[i] != '"')
        {
            temp_num[i] = pch[i];
            i++;
        }

        temp_num[i] = '\0'; //terminate the string
        //FileSize = strtoull (temp_num, NULL, 10); //convert to unsigned long long
        FileSize = stringToNumber ( (const char *) temp_num, &num_conversion);
        if (FALSE == num_conversion)
        {
            dbg (LOG_INFO, "Call to stringToNumber failed for FileSize with value '%s'", temp_num);
            ExitAndShowLog (1);
        }
    }
    else
    {
        FileSize = ReturnFileSize (fd);
    }

    if (FileSize == 0)
    {
        dbg (LOG_ERROR, "Filesize is 0 bytes. This is usually a mistake!! Please check '%s'", FileToSendWithPath);
        ExitAndShowLog (1);
    }

    BuildSizeTransferSoFar += FileSize; // BuildSizeTransferSoFar is manually reset before starting all of this

    if (SimulateForFileSize)
      return FIREHOSE_SUCCESS;  // above we know the file exists and we got the build size, now return

    if (FlattenBuild)
    {
        // to be here we are just copying the file to a local directory, and not actually doing the <program>

        // user provided this path, therefore save files into it
        memset (flattenbuildto, 0x0, sizeof (flattenbuildto) );

        if ( CopyString (flattenbuildto, flattenbuildvariant, 0, 0, strlen (flattenbuildvariant), sizeof (flattenbuildto), sizeof (flattenbuildvariant) )  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildto", flattenbuildvariant, strlen (flattenbuildvariant) );
            ExitAndShowLog (1);
        }

        if ( CopyString (flattenbuildto, fh.attrs.filename, strlen (flattenbuildto), 0, strlen (fh.attrs.filename), sizeof (flattenbuildto), strlen (fh.attrs.filename) )  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildto", fh.attrs.filename, strlen (fh.attrs.filename) );
            ExitAndShowLog (1);
        }
        result = MyCopyFile(FileToSendWithPath, flattenbuildto);
        if (result == -1) // this flatten build is for files possibly exluded by --notfiles --files etc
        {
            dbg (LOG_ERROR, "Failed to copy '%s'\n\t\t\tto '%s'\n\n", FileToSendWithPath, flattenbuildto);
            ExitAndShowLog (1);
        }
        else if (result == 0)
            dbg (LOG_INFO, "Copied '%s'\n\t\t\tto '%s'\n\n", FileToSendWithPath, flattenbuildto);

        if (showpercentagecomplete && TotalTransferSize > 0)
        {
          PercentageBuildLoaded = (float)((100.0 * (double)BuildSizeTransferSoFar) / (double)TotalTransferSize);
          dbg (LOG_INFO, "{percent files transferred %6.2f%%}", PercentageBuildLoaded);
        }

        return FIREHOSE_SUCCESS;
    }

    if (createconfigxml)
      return FIREHOSE_SUCCESS;  // above we know the file exists and we got the build size, now return

    if (verify_build)
      return FIREHOSE_SUCCESS;  // above we know the file exists and we got the build size, now return

    FileSizeNumSectors = FileSize / SectorSizeInBytes;

    if (FileSize % SectorSizeInBytes)
    {
        FileSizeNumSectors++;
    }


    dbg (LOG_DEBUG, "File size is %"SIZE_T_FORMAT" bytes", FileSize);
    dbg (LOG_DEBUG, "NumSectors needed %"SIZE_T_FORMAT, FileSizeNumSectors);

    if (FileSizeNumSectors == 0)
    {
        dbg (LOG_ERROR, "FileSizeNumSectors is 0. Nothing to program!!");
        ExitAndShowLog (1);
    }


    if (fh.attrs.num_partition_sectors == 0)
    {
        // this is usually a mistake. Something like last partition is set to grow and thus size was set to 0
        fh.attrs.num_partition_sectors = FileSizeNumSectors;
    }

    if (FileSizeNumSectors > fh.attrs.num_partition_sectors)
    {
        char FileSize1[2048], FileSize2[2048];

        ReturnSizeString (fh.attrs.num_partition_sectors * SectorSizeInBytes, FileSize1, sizeof (FileSize1) );
        ReturnSizeString (FileSize, FileSize2, sizeof (FileSize2) );

        dbg (LOG_WARN, "User requested to ONLY program num_partition_sectors %"SIZE_T_FORMAT" of '%s' (%s).\n"
             "To load this file completely would require %"SIZE_T_FORMAT" sectors (%s) and SectorSizeInBytes=%"SIZE_T_FORMAT"."
             "\nThis is usually a MISTAKE but there is a use case where you want to write only part of the image!\n",
             fh.attrs.num_partition_sectors, fh.attrs.filename, FileSize1, FileSizeNumSectors, FileSize2, SectorSizeInBytes);
        FileSizeNumSectors = fh.attrs.num_partition_sectors;
        //FileToSendWithPath
    }


    // NOTE: Can't send <program> tag as is since num_partition_sectors is most likely BIGGER than the filesize
    // tx_buffer already holds the XML file
    // but we need to modify it and we'll use temp_buffer for this
    // 1. Need to put my own TAG
    // 2. Need to put my own num_partition_sectors

    memset (temp_buffer, 0x0, FIREHOSE_TX_BUFFER_SIZE); // zero out to begin

    pch = strstr (tx_buffer, "<program" );

    if (pch == NULL)
    {
        dbg (LOG_ERROR, "missing TAG '<program ??? Should be impossible'\n");
        ExitAndShowLog (1);
    }

    TempLength = pch - tx_buffer;

    CopyString (temp_buffer, tx_buffer, 0, 0, TempLength, FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);

    if (ConvertProgram2Firmware)
        AppendToBuffer (temp_buffer, "<firmwarewrite ", FIREHOSE_TX_BUFFER_SIZE);
    else
        AppendToBuffer (temp_buffer, "<program ", FIREHOSE_TX_BUFFER_SIZE);

    TempLength += strlen ("<program "); // need to know where this ends

    pch = strstr (tx_buffer, "num_partition_sectors");

    if (pch == NULL)
    {
        dbg (LOG_ERROR, "<%s> tag is missing attribute 'num_partition_sectors'\n", OperationString[Index]);
        ExitAndShowLog (1);
    }

    if ( (SIZE_T) (pch - tx_buffer) > TempLength)
    {
        // to be here means there were more attributes between "<program "  and "num_partition_sectors"
        CopyString (temp_buffer, tx_buffer, strlen (temp_buffer), TempLength, (pch - tx_buffer) - TempLength, FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);
    }

    TempBufferSize = strlen (temp_buffer);

    //CopyString(temp_buffer, tx_buffer, strlen(temp_buffer), pch-tx_buffer, TempBufferSize, FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);
    sprintf (&temp_buffer[TempBufferSize], "num_partition_sectors=\"%"SIZE_T_FORMAT"\"", FileSizeNumSectors); // no space at end here!!

    pch = strstr (&tx_buffer[pch - tx_buffer], "\""); // looking for " in num_partition_sectors="1234"

    if (pch == NULL)
    {
        dbg (LOG_ERROR, "<%s> tag is missing first \" after 'num_partition_sectors'\n", OperationString[Index]);
        ExitAndShowLog (1);
    }

    TempBufferSize = pch - tx_buffer + 1; // +1 gets us past the first "

    pch = strstr (&tx_buffer[TempBufferSize], "\""); // looking for " in num_partition_sectors="1234"

    if (pch == NULL)
    {
        dbg (LOG_ERROR, "<%s> tag is missing second \" after 'num_partition_sectors'\n", OperationString[Index]);
        ExitAndShowLog (1);
    }

    TempBufferSize = pch - tx_buffer + 1; // +1 gets us past the second "

    AppendToBuffer (temp_buffer, &tx_buffer[TempBufferSize], FIREHOSE_TX_BUFFER_SIZE);

    memscpy (tx_buffer, FIREHOSE_TX_BUFFER_SIZE, temp_buffer, FIREHOSE_TX_BUFFER_SIZE); // memcpy

    /*
      InitBufferWithXMLHeader(tx_buffer, sizeof(tx_buffer));

      // HACK, to match Zeno
      CopyString(tx_buffer, "<?xml version=\"1.0\" ?>\n", 0, 0, strlen("<?xml version=\"1.0\" ?>\n"), FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);

        AppendToBuffer(tx_buffer,"<data>\n",FIREHOSE_TX_BUFFER_SIZE);
        AppendToBuffer(tx_buffer,"  <program ",FIREHOSE_TX_BUFFER_SIZE);  // HACK, 2 spaces are to match Zeno

        sprintf(temp_buffer,"SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ",fh.attrs.SECTOR_SIZE_IN_BYTES);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"filename=\"dummy64.bin\" label=\"single_file\" "); // HACK, DeviceProg doesn't require this info
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"num_partition_sectors=\"%"SIZE_T_FORMAT"\" ",FileSizeNumSectors);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"physical_partition_number=\"%"SIZE_T_FORMAT"\" ",fh.attrs.physical_partition_number);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        //sprintf(temp_buffer,"start_sector=\"%s\" ",fh.attrs.start_sector);
      sprintf(temp_buffer,"start_sector=\"%s\"",fh.attrs.start_sector); // HACK, in Zeno I don't have a space at the end
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        AppendToBuffer(tx_buffer,"/>\n</data>\n",FIREHOSE_TX_BUFFER_SIZE);  // HACK, \n is for Zeno
    */

    if (UsingValidation && skipharddriveread)
        sprintf (MyTempString, " **Using VIP**\n\t\t** NOTE: skipharddriveread=1 (will use accurate filesize but all 0xFF data)");
    else if (skipharddriveread)
        sprintf (MyTempString, "\n\t\t** NOTE: skipharddriveread=1 (will use accurate filesize but all 0xFF data)");
    else if (UsingValidation)
        sprintf (MyTempString, " **Using VIP**");

    dbg (LOG_ALWAYS, "=======================================================");
    dbg (LOG_ALWAYS, "{<%s> FILE: '%s'%s}", OperationString[Index], FileToSendWithPath, MyTempString);

    ReturnSizeString (FileSize, MyTempString, sizeof (MyTempString) );

    dbg (LOG_ALWAYS, "{<%s> (%s) %"SIZE_T_FORMAT" sectors needed at location %i on LUN %"SIZE_T_FORMAT"}", OperationString[Index], MyTempString, FileSizeNumSectors, atoi (fh.attrs.start_sector), fh.attrs.physical_partition_number);
    dbg (LOG_ALWAYS, "=======================================================\n");

    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" rawmode=\"true\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    GetNextPacket();  // this will set all variables, including GotACK

    if (!GotACK)
    {
        dbg (LOG_ERROR, "%s FAILED - Please see log", OperationString[Index]);
        ExitAndShowLog (1);
    }


    //fread(EntireXMLFileBuffer, XMLFileSize, 1, fd);
    printf ("\n");

    gettimeofday (&time_start, NULL);
    AbsoluteTimeStart = time_start;

    // Initialize SHA256
    if (verify_programming || ShowDigestPerFile)
    {
        SizeOfDataFedToHashRoutine = 0; // reset this
        sechsharm_sha256_init (&context);
    }

    RawMode = 1;

    // Request from corporate tools is to show percentage complete per packet
    BuildSizeTransferSoFar -= FileSize; // this undoes above, will += this below again

    if (isSparseFile)
    {
        sparse_header = sparse_read_handle_init (fd, fh.attrs.file_sector_offset, FileSize, &sparse_read_handle);
    }
    else
    {
        fseek(fd, fh.attrs.file_sector_offset*fh.attrs.SECTOR_SIZE_IN_BYTES, SEEK_SET); //seek to the correct position in the file
    }
    while (FileSizeNumSectors > 0)
    {
        gettimeofday (&network_start, NULL);

        if (!skipharddriveread)
        {
            if (isSparseFile)
            {
                BytesRead = sparse_read (tx_buffer, fh.attrs.MaxPayloadSizeToTargetInBytes, fd, &sparse_read_handle, sparse_header);
            }
            else
            {
                BytesRead = fread (&tx_buffer, sizeof (char), fh.attrs.MaxPayloadSizeToTargetInBytes, fd); // read from hard drive
            }
        }
        else
        {
            BytesRead = fh.attrs.MaxPayloadSizeToTargetInBytes; // fake it
            memset (tx_buffer, 0xFF, sizeof (tx_buffer) );
        }

        if (BytesRead == 0)
        {
            dbg (LOG_ERROR, "Read 0 bytes from '%s'", FileToSendWithPath);
            dbg (LOG_ERROR, "There are still %"SIZE_T_FORMAT" sectors to go", FileSizeNumSectors);
            ExitAndShowLog (1);
        }

        BuildSizeTransferSoFar += BytesRead;

        gettimeofday (&time_end, NULL);

        NumSectors = BytesRead / fh.attrs.SECTOR_SIZE_IN_BYTES;

        if (BytesRead % fh.attrs.SECTOR_SIZE_IN_BYTES)
        {
            // Need to zero pad to fill last sector
            NumSectors++;

            //fh.attrs.SECTOR_SIZE_IN_BYTES             = 512
            //(BytesRead%fh.attrs.SECTOR_SIZE_IN_BYTES) = 168
            // Therefore need 512-168=344 zeros

            for (i = 0; i < fh.attrs.SECTOR_SIZE_IN_BYTES - (BytesRead % fh.attrs.SECTOR_SIZE_IN_BYTES); i++)
                tx_buffer[i + BytesRead] = 0; // zero padding to 1 full sector
        }

        // Now Send tx_buffer
        if (NumSectors > FileSizeNumSectors)
            NumSectors = FileSizeNumSectors;  // we read more file than user wants to send, so truncate to this


        TotalSectorsFlashed += NumSectors;
        NetworkElapsed += ReturnTimeInSeconds (&network_start, &time_end);
        ReturnSizeString ( (SIZE_T) (TotalSectorsFlashed * fh.attrs.SECTOR_SIZE_IN_BYTES), temp_buffer, sizeof (temp_buffer) );
        //printf("\n%s read in NetworkElapsed=%f",temp_buffer,NetworkElapsed);

        if (ReturnTimeInSeconds (&time_start, &time_end) >= 2.0)
        {
            time_throughput_calculate (&AbsoluteTimeStart, &time_end, TotalSectorsFlashed * fh.attrs.SECTOR_SIZE_IN_BYTES, NetworkElapsed, LOG_INFO );
            gettimeofday (&time_start, NULL);

            if (VerboseLevel == LOG_ALWAYS) // this means user wants minimal logging
                printf (".");
        }

        if ( sendTransmitBufferBytes (NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES) == 0)
        {
            return FIREHOSE_ERROR;
        }

        FileSizeNumSectors -= NumSectors;

        if (verify_programming || ShowDigestPerFile)
        {
            SizeOfDataFedToHashRoutine += (NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES);
            sechsharm_sha256_update (&context, context.leftover, & (context.leftover_size), (unsigned char *) tx_buffer, NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES);
        }

    } // end of while(FileSizeNumSectors>0)


    RawMode = 0; // we are out, treat all incoming data now as XML again

    gettimeofday (&time_end, NULL);

    FileSizeNumSectors = FileSize / SectorSizeInBytes;

    if (FileSize % SectorSizeInBytes)
    {
        FileSizeNumSectors++;
    }

    if (FileSizeNumSectors > fh.attrs.num_partition_sectors)
        FileSizeNumSectors = fh.attrs.num_partition_sectors;

    time_throughput_calculate (&AbsoluteTimeStart, &time_end, FileSizeNumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES, 0.0, LOG_ALWAYS );

    // Finalize SHA256
    if (verify_programming || ShowDigestPerFile)
    {
        sechsharm_sha256_final (&context, context.leftover, & (context.leftover_size), last_hash_value);
        PrettyPrintHexValueIntoTempBuffer (last_hash_value, 32, 0, 32); // from, size, offset, maxlength
        printf ("\n");
        dbg (LOG_INFO, "verify_programming '%s'\nSHA256 (%7d bytes) %s\n", FileToSendWithPath, SizeOfDataFedToHashRoutine, temp_buffer);
        memscpy (verify_hash_value, sizeof (verify_hash_value), last_hash_value, sizeof (last_hash_value) ); // memcpy

        if (ShowDigestPerFile)
            fprintf (fdp, "'%s' --> LUN %"SIZE_T_FORMAT" start_sector %s SHA256 (%"SIZE_T_FORMAT" bytes) %s\n", FileToSendWithPath, fh.attrs.physical_partition_number, fh.attrs.start_sector, SizeOfDataFedToHashRoutine, temp_buffer);
    }

    //printf("\n");

    fclose (fd);
    fd = NULL;

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" rawmode=\"false\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    fflush (fp); // flush to port_trace.txt

    GetNextPacket();  // this will set all variables, including GotACK

    if (!GotACK)
    {
        dbg (LOG_ERROR, "Please see log");
        ExitAndShowLog (1);
    }


    // =========================================== VERIFY PROGRAMMING HAPPENS HERE IF REQUESTED ====================================
    // =========================================== VERIFY PROGRAMMING HAPPENS HERE IF REQUESTED ====================================
    // =========================================== VERIFY PROGRAMMING HAPPENS HERE IF REQUESTED ====================================

    if (wipefirst && strncmp (fh.attrs.filename, WipeFirstFileName, MAX (strlen (fh.attrs.filename), strlen (WipeFirstFileName) ) ) == 0 )
    {
        // to be here means this is the Zeros16KB.bin file I write with --wipefirst
        // so do nothing with regards to verify_programming, i.e. I don't want to read back the file of all zeros
    }
    else if (verify_programming)
    {


        InitBufferWithXMLHeader (tx_buffer, sizeof (tx_buffer) );
        AppendToBuffer (tx_buffer, "<data>\n", FIREHOSE_TX_BUFFER_SIZE);
        //AppendToBuffer(tx_buffer,"<getsha256digest ",FIREHOSE_TX_BUFFER_SIZE);
        AppendToBuffer (tx_buffer, "<read ", FIREHOSE_TX_BUFFER_SIZE);


        //CopyString(FileToReadBackWithPath, cwd, 0, 0, strlen(cwd), MAX_PATH_SIZE, MAX_PATH_SIZE);
        //CopyString(FileToReadBackWithPath, fh.attrs.filename, strlen(FileToReadBackWithPath), 0, strlen(cwd), MAX_PATH_SIZE, MAX_PATH_SIZE);


        //sprintf(temp_buffer,"filename=\"%s\" ",FileToReadBackWithPath);
        // There can be no path on this name
        sprintf (temp_buffer, "filename=\"%s\" ", fh.attrs.filename);

        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ", fh.attrs.SECTOR_SIZE_IN_BYTES);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "num_partition_sectors=\"%"SIZE_T_FORMAT"\" ", FileSizeNumSectors);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "physical_partition_number=\"%"SIZE_T_FORMAT"\" ", fh.attrs.physical_partition_number);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        sprintf (temp_buffer, "start_sector=\"%s\" ", fh.attrs.start_sector);
        AppendToBuffer (tx_buffer, temp_buffer, FIREHOSE_TX_BUFFER_SIZE);

        AppendToBuffer (tx_buffer, "/>\n</data>", FIREHOSE_TX_BUFFER_SIZE);

        memscpy (temp_buffer3, FIREHOSE_TX_BUFFER_SIZE, tx_buffer, strlen (tx_buffer) ); // memcpy
        /*
        dbg(LOG_ALWAYS,"=======================================================");
        dbg(LOG_ALWAYS,"<getsha256> FILE: '%s'",FileToSendWithPath);
        dbg(LOG_ALWAYS,"=======================================================\n\n");

        sendTransmitBuffer();
        */
        SendXMLString (temp_buffer3, strlen (temp_buffer3) );

        if (Simulate)
        {
            memscpy (last_hash_value, sizeof (last_hash_value), verify_hash_value, sizeof (verify_hash_value) ); // memcpy
        } // end if(Simulate)
        else
        {
            // At this point the file FileToReadBackWithPath has been created
            //fd = ReturnFileHandle(FileToReadBackWithPath, MAX_PATH_SIZE, "rb"); // will exit if not successful
            fd = ReturnFileHandle (fh.attrs.filename, MAX_PATH_SIZE, "rb"); // will exit if not successful
            FileSize = ReturnFileSize (fd);

            if (FileSize == 0)
            {
                //dbg(LOG_ERROR, "Filesize is 0 bytes for '%s'. Previous <read> failed??",FileToReadBackWithPath);
                dbg (LOG_ERROR, "Filesize is 0 bytes for '%s'. Previous <read> failed??", fh.attrs.filename);
                ExitAndShowLog (1);
            }


            // Initialize SHA256
            SizeOfDataFedToHashRoutine = 0; // reset this
            sechsharm_sha256_init (&context);

            memset (tx_buffer, 0x0, FIREHOSE_TX_BUFFER_SIZE);

            dbg (LOG_INFO, "Calculating SHA256");

            while (FileSize > FIREHOSE_TX_BUFFER_SIZE)
            {
                BytesRead = fread (&tx_buffer, sizeof (char), FIREHOSE_TX_BUFFER_SIZE, fd); // read from hard drive
                SizeOfDataFedToHashRoutine += (SIZE_T) FIREHOSE_TX_BUFFER_SIZE;
                sechsharm_sha256_update (&context, context.leftover, & (context.leftover_size), (unsigned char *) tx_buffer, FIREHOSE_TX_BUFFER_SIZE);
                FileSize -= FIREHOSE_TX_BUFFER_SIZE;
                printf (".");
            }

            // get what is left
            BytesRead = fread (&tx_buffer, sizeof (char), FileSize, fd); // read from hard drive
            SizeOfDataFedToHashRoutine += FileSize;
            sechsharm_sha256_update (&context, context.leftover, & (context.leftover_size), (unsigned char *) tx_buffer, FileSize);

            fclose (fd);
            fd = NULL;


            sechsharm_sha256_final (&context, context.leftover, & (context.leftover_size), last_hash_value);
            PrettyPrintHexValueIntoTempBuffer (last_hash_value, 32, 0, 32); // from, size, offset, maxlength
        }

        printf ("\n");
        //dbg(LOG_INFO, "verify_programming FROM TARGET '%s'\nSHA256 (%7d bytes) %s\n",FileToReadBackWithPath,SizeOfDataFedToHashRoutine,temp_buffer);
        dbg (LOG_INFO, "verify_programming FROM TARGET '%s'\nSHA256 (%7d bytes) %s\n", fh.attrs.filename, SizeOfDataFedToHashRoutine, temp_buffer);


        for (i = 0; i < 32; i++)
        {
            if (verify_hash_value[i] != last_hash_value[i])
                break;
        }

        i = i; // for breakpoint


        if (i == 32)
        {
            //printf("MATCHED - '%s'\n",AllAttributes[i].Name);
            dbg (LOG_ALWAYS, "  __ _           _     _            ");
            dbg (LOG_ALWAYS, " / _| |         | |   (_)            ");
            dbg (LOG_ALWAYS, "| |_| | __ _ ___| |__  _ _ __   __ _ ");
            dbg (LOG_ALWAYS, "|  _| |/ _` / __| '_ \\| | '_ \\ / _` |");
            dbg (LOG_ALWAYS, "| | | | (_| \\__ \\ | | | | | | | (_| |");
            dbg (LOG_ALWAYS, "|_| |_|\\__,_|___/_| |_|_|_| |_|\\__, |");
            dbg (LOG_ALWAYS, "                                __/ |");
            dbg (LOG_ALWAYS, "                               |___/ ");
            dbg (LOG_ALWAYS, "                    _                ");
            dbg (LOG_ALWAYS, "                   | |               ");
            dbg (LOG_ALWAYS, "__      _____  _ __| | _____     ");
            dbg (LOG_ALWAYS, "\\ \\ /\\ / / _ \\| '__| |/ / __|        ");
            dbg (LOG_ALWAYS, " \\ V  V / (_) | |  |   <\\__ \\        ");
            dbg (LOG_ALWAYS, "  \\_/\\_/ \\___/|_|  |_|\\_\\___/        \n");
            dbg (LOG_ALWAYS, "FLASHING WORKS. This tool *is* able to successfully flash data onto this device\n\n");
        }
        else
        {
            dbg (LOG_ALWAYS, "  __ _           _     _            ");
            dbg (LOG_ALWAYS, " / _| |         | |   (_)            ");
            dbg (LOG_ALWAYS, "| |_| | __ _ ___| |__  _ _ __   __ _ ");
            dbg (LOG_ALWAYS, "|  _| |/ _` / __| '_ \\| | '_ \\ / _` |");
            dbg (LOG_ALWAYS, "| | | | (_| \\__ \\ | | | | | | | (_| |");
            dbg (LOG_ALWAYS, "|_| |_|\\__,_|___/_| |_|_|_| |_|\\__, |");
            dbg (LOG_ALWAYS, "                                __/ |");
            dbg (LOG_ALWAYS, "                               |___/ ");
            dbg (LOG_ALWAYS, "  __      _ _          _            ");
            dbg (LOG_ALWAYS, " / _|    (_) |        | |            ");
            dbg (LOG_ALWAYS, "| |_ __ _ _| | ___  __| |          ");
            dbg (LOG_ALWAYS, "|  _/ _` | | |/ _ \\/ _` |            ");
            dbg (LOG_ALWAYS, "| || (_| | | |  __/ (_| |            ");
            dbg (LOG_ALWAYS, "|_| \\__,_|_|_|\\___|\\__,_|                                   \n");
            dbg (LOG_ALWAYS, "FLASHING FAILED. This tool was *NOT* able to flash data onto this device\n\n");
            ExitAndShowLog (1);
        }

        // Finally, rename fh.attrs.filename as "temp_file" to avoid accidently finding this file the next time the user wants to program

        //ret = remove("tempfile.bin");
        //ret = rename(FileToReadBackWithPath, "tempfile.bin");
        //ret = rename(fh.attrs.filename, "tempfile.bin");

        if (ret == 0)
        {
            dbg (LOG_ALWAYS, "Just renamed the read back file '%s' to '%s'", fh.attrs.filename, "tempfile.bin");
        }
        else
        {
            dbg (LOG_ERROR, "unable to rename the file '%s' to '%s' errno=%d", fh.attrs.filename, "tempfile.bin", errno);
            ExitAndShowLog (1);
        }

    } // end if(verify_programming)

    // =========================================== DONE VERIFY PROGRAMMING ====================================
    // =========================================== DONE VERIFY PROGRAMMING ====================================
    // =========================================== DONE VERIFY PROGRAMMING ====================================

    /*
      printf("\n\nTESTING TIMING\n");
      gettimeofday(&time_start, NULL);
      sleep(5000);
      gettimeofday(&time_end, NULL);
      timeval_subtract(&time_result, &time_end, &time_start);
      TimeInSeconds = (double)time_result.tv_sec + (double)(time_result.tv_usec/1000000.0);
      printf("\n\nDONE TESTING TIMING TimeInSeconds=%f\n",TimeInSeconds);
    */




    if (!GotACK)
    {
        dbg (LOG_ERROR, "Please see log");
        ExitAndShowLog (1);
    }

    dbg (LOG_INFO, "");
    dbg (LOG_INFO, "=======================================================");
    dbg (LOG_INFO, "==================== {SUCCESS} ========================");
    dbg (LOG_INFO, "=======================================================\n\n");

    if (SimulateForFileSize == 0 && TotalTransferSize != 0)
    {
        if (TotalTransferSize > 0)
            PercentageBuildLoaded = (float) ( (100.0 * (double) BuildSizeTransferSoFar) / (double) TotalTransferSize);

        dbg (LOG_INFO, "{percent files transferred %6.2f%%}", PercentageBuildLoaded);
    }


    fflush (fp); // flush to port_trace.txt

    return FIREHOSE_SUCCESS;
}


static firehose_error_t handleRead()
{
    SIZE_T FileSizeNumSectors = 0, FileSizeNumSectorsLeft = 0, BytesRead = 0, NumSectors = 0, i, TotalSectorsFlashed = 0, WriteCount = 0;
    char *FileToSendWithPath = NULL;
    FILE *fr, *fd;
    struct timeval time_start, time_end, AbsoluteTimeStart;
    double TimeInSeconds = 0.0;
    SIZE_T FileSize = 0, TempBufferSize = 0;


    for (i = 0; i < strlen (fh.attrs.filename); i++)
    {
        if (IsEmptySpace (fh.attrs.filename[i]) )
            fh.attrs.filename[i] = '\0';
    }

    if (strlen (fh.attrs.filename) == 0)
        return FIREHOSE_SUCCESS;


    if (num_filter_files > 0)
    {
        if (!ThisFileIsInFilterFiles (fh.attrs.filename) )
        {
            dbg (LOG_DEBUG, "'%s' is being SKIPPED since it was not in --files provided by the user at the command line", fh.attrs.filename);
            return FIREHOSE_SUCCESS;
        }
    }

    if (num_filter_not_files > 0)
    {
        if (ThisFileIsInNotFilterFiles (fh.attrs.filename) )
        {
            dbg (LOG_INFO, "handleProgram() '%s' is being SKIPPED since it was in --notfiles provided by the user at the command line", fh.attrs.filename);
            return FIREHOSE_SUCCESS;
        }
    }

    if (createconfigxml || SimulateForFileSize || verify_build)
    {
        // don't need to print anything, it's just messy
    }
    else
    {
        dbg (LOG_INFO, "In handleRead('%s')", fh.attrs.filename);
    }

// -----------------------------------------------------------------------------------------------------------------------------------------
// Step 1 is to see if the original file exists, in that case, user probably wants to compare to the original

    // There is no FileSize in this case since we can only read the number of sectors the partition has
    FileSizeNumSectors    = fh.attrs.num_partition_sectors;

    FileToSendWithPath = find_file (fh.attrs.filename, 1);

    if (FileToSendWithPath != NULL)
    {
        if (fh.attrs.SECTOR_SIZE_IN_BYTES != SectorSizeInBytes)
        {
            SectorSizeInBytes = fh.attrs.SECTOR_SIZE_IN_BYTES;
            dbg (LOG_INFO, "SectorSizeInBytes changed to %d", SectorSizeInBytes);
        }

        fd = ReturnFileHandle (FileToSendWithPath, MAX_PATH_SIZE, "rb"); // will exit if not successful

        FileSize = ReturnFileSize (fd);
        fclose (fd);
        fd = NULL;

        if (FileSize == 0)
        {
            FileSize = fh.attrs.num_partition_sectors * SectorSizeInBytes;
            dbg (LOG_WARN, "Previous Filesize is 0 bytes. Therefore reading size of partition!! Please check '%s'", FileToSendWithPath);
        }

        FileSizeNumSectors = FileSize / SectorSizeInBytes;

        if (FileSize % SectorSizeInBytes)
        {
            FileSizeNumSectors++;
        }

        dbg (LOG_DEBUG, "File size is %"SIZE_T_FORMAT" bytes", FileSize);
        dbg (LOG_DEBUG, "NumSectors needed %"SIZE_T_FORMAT, FileSizeNumSectors);
    }


    // this might have been updated above
    FileSizeNumSectorsLeft  = FileSizeNumSectors;

// -----------------------------------------------------------------------------------------------------------------------------------------

    // to be this far we are in RAW MODE. Need to read the file
    fr = ReturnFileHandle (fh.attrs.filename, MAX_PATH_SIZE, "wb"); // will exit if not successful

    if (fh.attrs.SECTOR_SIZE_IN_BYTES != SectorSizeInBytes)
    {
        SectorSizeInBytes = fh.attrs.SECTOR_SIZE_IN_BYTES;
        dbg (LOG_INFO, "SectorSizeInBytes changed to %d", SectorSizeInBytes);
    }

    if (SimulateForFileSize)
    {
        BuildSizeTransferSoFar += FileSize; // BuildSizeTransferSoFar is manually reset before starting all of this
        return FIREHOSE_SUCCESS;  // above we know the file exists and we got the build size, now return
    }

    if (FileSize > 0)
    {
        // original file exists
        char *pch;

        memset (temp_buffer, 0x0, FIREHOSE_TX_BUFFER_SIZE); // zero out to begin

        pch = strstr (tx_buffer, "num_partition_sectors");

        if (pch == NULL)
        {
            dbg (LOG_ERROR, "<read> tag is missing attribute 'num_partition_sectors'\n");
            ExitAndShowLog (1);
        }

        CopyString (temp_buffer, tx_buffer, 0, 0, (pch - tx_buffer), FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);

        TempBufferSize = strlen (temp_buffer);

        //CopyString(temp_buffer, tx_buffer, strlen(temp_buffer), pch-tx_buffer, TempBufferSize, FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);
        sprintf (&temp_buffer[TempBufferSize], "num_partition_sectors=\"%"SIZE_T_FORMAT"\"", FileSizeNumSectors); // no space at end here!!

        pch = strstr (&tx_buffer[pch - tx_buffer], "\""); // looking for " in num_partition_sectors="1234"

        if (pch == NULL)
        {
            dbg (LOG_ERROR, "<read> tag is missing first \" after 'num_partition_sectors'\n");
            ExitAndShowLog (1);
        }

        TempBufferSize = pch - tx_buffer + 1; // +1 gets us past the first "

        pch = strstr (&tx_buffer[TempBufferSize], "\""); // looking for " in num_partition_sectors="1234"

        if (pch == NULL)
        {
            dbg (LOG_ERROR, "<read> tag is missing second \" after 'num_partition_sectors'\n");
            ExitAndShowLog (1);
        }

        TempBufferSize = pch - tx_buffer + 1; // +1 gets us past the second "

        AppendToBuffer (temp_buffer, &tx_buffer[TempBufferSize], FIREHOSE_TX_BUFFER_SIZE);

        memscpy (tx_buffer, FIREHOSE_TX_BUFFER_SIZE, temp_buffer, FIREHOSE_TX_BUFFER_SIZE); // memcpy

    } // original file exists


    // tx_buffer already holds the XML file
    sendTransmitBuffer();


    /*
      InitBufferWithXMLHeader(tx_buffer, sizeof(tx_buffer));
        AppendToBuffer(tx_buffer,"<data>\n",FIREHOSE_TX_BUFFER_SIZE);
        AppendToBuffer(tx_buffer,"<read ",FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"SECTOR_SIZE_IN_BYTES=\"%"SIZE_T_FORMAT"\" ",fh.attrs.SECTOR_SIZE_IN_BYTES);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"num_partition_sectors=\"%"SIZE_T_FORMAT"\" ",FileSizeNumSectors);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"physical_partition_number=\"%"SIZE_T_FORMAT"\" ",fh.attrs.physical_partition_number);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        sprintf(temp_buffer,"start_sector=\"%s\" ",fh.attrs.start_sector);
        AppendToBuffer(tx_buffer,temp_buffer,FIREHOSE_TX_BUFFER_SIZE);

        AppendToBuffer(tx_buffer,"/>\n</data>",FIREHOSE_TX_BUFFER_SIZE);
    */
    dbg (LOG_ALWAYS, "=======================================================");
    dbg (LOG_ALWAYS, "<read> (%.1fKB) %"SIZE_T_FORMAT" sectors from location %s FILE: '%s'", (double) (FileSizeNumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES) / 1024.0, FileSizeNumSectors, fh.attrs.start_sector, fh.attrs.filename);
    dbg (LOG_ALWAYS, "=======================================================");
    //sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" rawmode=\"true\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    RemoveCommentsFromXMLFiles = 0; // Unlikely but possible we are reading an XML file from the eMMC
    // thus getnextpacket below will cleans the ReadBuffer of comments

    GetNextPacket();  // this will set all variables, including GotACK

    if (!GotACK)
    {
        dbg (LOG_ERROR, "Please see log");
        ExitAndShowLog (1);
    }

    printf ("\n");

    // Initialize SHA256
    if (ShowDigestPerFile)
    {
        SizeOfDataFedToHashRoutine = 0; // reset this
        sechsharm_sha256_init (&context);
    }


    RawMode = 1;

    gettimeofday (&time_start, NULL);
    AbsoluteTimeStart = time_start;

    while (RawMode > 0)
    {
        if (MaxBytesToReadFromUSB > FileSizeNumSectorsLeft * fh.attrs.SECTOR_SIZE_IN_BYTES)
            MaxBytesToReadFromUSB = FileSizeNumSectorsLeft * fh.attrs.SECTOR_SIZE_IN_BYTES;

        if (MaxBytesToReadFromUSB > MAX_READ_BUFFER_SIZE)
            MaxBytesToReadFromUSB = MAX_READ_BUFFER_SIZE;

        if (Simulate)
            CharsInBuffer = MaxBytesToReadFromUSB;

        dbg (LOG_DEBUG, "FileSizeNumSectorsLeft = %"SIZE_T_FORMAT, FileSizeNumSectorsLeft);

        GetNextPacket();  // In RawMode this will return without setting any attributes

        while (CharsInBuffer < fh.attrs.SECTOR_SIZE_IN_BYTES)
        {
            // add logic to handle scenario where recived data is less that sector size.
            MaxBytesToReadFromUSB -= CharsInBuffer;
            GetNextPacket();
        }

        NumSectors        = CharsInBuffer / fh.attrs.SECTOR_SIZE_IN_BYTES;
        BuildSizeTransferSoFar += CharsInBuffer;

        if (NumSectors == 0)
        {
            dbg (LOG_ERROR, "In HandleRead ** NOTHING READ FROM TARGET ** Can't continue if not getting data. CharsInBuffer=%i and fh.attrs.SECTOR_SIZE_IN_BYTES=%"SIZE_T_FORMAT, CharsInBuffer, fh.attrs.SECTOR_SIZE_IN_BYTES);
            PRETTYPRINT ( (unsigned char*) ReadBuffer, PacketLoc + CharsInBuffer, MAX_READ_BUFFER_SIZE); // always show everything
            ExitAndShowLog (1);
        }

        // NOTE: On the last RAW packet there is a chance we have XML data in there too
        if (NumSectors > FileSizeNumSectorsLeft)
            NumSectors = FileSizeNumSectorsLeft;  // this is what was left of RawData

        WriteCount++;
        fwrite (ReadBuffer, NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES, 1, fr);
        //    dbg(LOG_DEBUG,"Just called fwrite with %"SIZE_T_FORMAT" CharsInBuffer=%ld\n",WriteCount,CharsInBuffer);

        //dbg(LOG_INFO,"FileSizeNumSectorsLeft = %"SIZE_T_FORMAT,FileSizeNumSectorsLeft);

        gettimeofday (&time_end, NULL);

        TotalSectorsFlashed   += NumSectors;

        if (NumSectors < FileSizeNumSectorsLeft)    // avoiding a roll over
            FileSizeNumSectorsLeft  -= NumSectors;
        else
            FileSizeNumSectorsLeft   = 0;

        if (FileSizeNumSectorsLeft <= 0)
            RawMode = 0;

        //printf("\n\nNEW FileSizeNumSectorsLeft=%"SIZE_T_FORMAT"\n",FileSizeNumSectorsLeft);

        PacketLoc     += NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES;
        CharsInBuffer -= NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES;

        if (CharsInBuffer <= 0)
        {
            CharsInBuffer     = 0;
            PacketLoc       = 0;
        }


        if (ReturnTimeInSeconds (&time_start, &time_end) >= 2.0)
        {
            // To be here means more than 2 seconds has passed

            if (TotalSectorsFlashed > 0)
            {
                time_throughput_calculate (&AbsoluteTimeStart, &time_end, TotalSectorsFlashed * fh.attrs.SECTOR_SIZE_IN_BYTES, 0.0, LOG_INFO );
            }

            //gettimeofday(&time_start, NULL);
            time_start = time_end;

            if (VerboseLevel == LOG_ALWAYS) // this means user wants minimal logging
                printf (".");
        }


        if (ShowDigestPerFile)
        {
            SizeOfDataFedToHashRoutine += (NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES);
            sechsharm_sha256_update (&context, context.leftover, & (context.leftover_size), (unsigned char *) ReadBuffer, NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES);
        }

    } // end of while(RawMode>0)

    gettimeofday (&time_end, NULL);

    RawMode = 0;


    time_throughput_calculate (&AbsoluteTimeStart, &time_end, FileSizeNumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES, 0.0, LOG_ALWAYS );

    // Finalize SHA256
    if (ShowDigestPerFile)
    {
        sechsharm_sha256_final (&context, context.leftover, & (context.leftover_size), last_hash_value);
        PrettyPrintHexValueIntoTempBuffer (last_hash_value, 32, 0, 32); // from, size, offset, maxlength
        printf ("\n");
        dbg (LOG_INFO, "verify_programming '%s'\nSHA256 (%7d bytes) %s\n", fh.attrs.filename, SizeOfDataFedToHashRoutine, temp_buffer);
        memscpy (verify_hash_value, sizeof (verify_hash_value), last_hash_value, sizeof (last_hash_value) ); // memcpy

        if (ShowDigestPerFile)
            fprintf (fdp, "'%s' --> LUN %"SIZE_T_FORMAT" start_sector %s SHA256 (%"SIZE_T_FORMAT" bytes) %s\n", FileToSendWithPath, fh.attrs.physical_partition_number, fh.attrs.start_sector, SizeOfDataFedToHashRoutine, temp_buffer);

    }

    fclose (fr);
    fr = NULL;

    // Restored this to normal
    MaxBytesToReadFromUSB = fh.attrs.MaxPayloadSizeFromTargetInBytes;

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" rawmode=\"false\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }


    GetNextPacket();  // this will set all variables, including GotACK

    RemoveCommentsFromXMLFiles = 1; // turn this back on



    if (!GotACK)
    {
        dbg (LOG_ERROR, "Please see log");
        ExitAndShowLog (1);
    }


    dbg (LOG_INFO, "=======================================================");
    dbg (LOG_INFO, "===================== SUCCESS =========================");
    dbg (LOG_INFO, "=======================================================\n\n");

    return FIREHOSE_SUCCESS;
}


SIZE_T ReturnFileSize (FILE *fd)
{
    SIZE_T FileSize = 0;
    dbg (LOG_DEBUG, "Trying get filesize, calling fseek()");
    fseek (fd, 0, SEEK_END);  // seek to end of file
    FileSize = ftell (fd); // get current file pointer
    fseek (fd, 0, SEEK_SET);  // seek back to beginning of file
    return (SIZE_T) FileSize;
}

int gettimeofday (struct timeval *t, void* tzp)
{
#ifdef _MSC_VER // i.e. if compiling under Windows
    unsigned long system_uptime = GetTickCount();
    t->tv_sec = system_uptime / 1000;
    t->tv_usec = (system_uptime % 1000) * 1000;
#else
    struct timespec ts;

    if (clock_gettime (CLOCK_MONOTONIC, &ts) != 0)
    {
        dbg (LOG_ERROR, "clock_gettime() failed");
        ExitAndShowLog (1);
    }

    t->tv_sec = ts.tv_sec;
    t->tv_usec = ts.tv_nsec / 1000; // nano to microseconds

    //printf("\n\nt->tv_sec  = %"SIZE_T_FORMAT, t->tv_sec);
    //printf("\n\nt->tv_usec = %"SIZE_T_FORMAT, t->tv_usec);
#endif
    return 0;
}

double ReturnTimeInSeconds (struct timeval *start_time, struct timeval *end_time)
{
    struct timeval result;
    double TimeInSeconds = 0.0;

    timeval_subtract (&result, end_time, start_time);

    TimeInSeconds = (double) result.tv_usec;
    TimeInSeconds = TimeInSeconds / 1000000.0;
    TimeInSeconds += (double) result.tv_sec;

    return TimeInSeconds;
}

void time_throughput_calculate (struct timeval *start_time, struct timeval *end_time, size_t size_bytes, double NetworkElapsed, enum LOG_TYPES ThisLogType)
{
    struct timeval result;
    double TimeInSeconds = 0.0, NetworkSlow = 0.0;
    char SizeString1[2048];
    char SizeString2[2048];
    char SizeString3[2048] = "Network Speed ";

    timeval_subtract (&result, end_time, start_time);

    TimeInSeconds = (double) result.tv_sec + (double) (result.tv_usec / 1000000.0);

    //TimeInSeconds = ReturnTimeInSeconds(start_time,end_time);

    ReturnSizeString ( (SIZE_T) (size_bytes), SizeString2, sizeof (SizeString2) );

    //dbg(LOG_INFO, "%d bytes transferred in %"SIZE_T_FORMAT".%06ld seconds", size_bytes, result.tv_sec, result.tv_usec);
    //dbg(LOG_INFO, "%d bytes transferred in %f seconds", size_bytes, TimeInSeconds);
    if (TimeInSeconds == 0.0)
        TimeInSeconds = 0.01;

    if (NetworkElapsed == 0.0)
        NetworkElapsed = 0.015;

    NetworkSlow = (double) size_bytes / NetworkElapsed;

    if (NetworkSlow < 100000000.0)
    {
        ReturnSizeString ( (SIZE_T) (size_bytes / NetworkElapsed), SizeString1, sizeof (SizeString1) );
        sprintf (SizeString3, "FILE ACCESS SLOW!! %10s in %6.3f seconds (%10sps) --- Overall to target", SizeString2, NetworkElapsed, SizeString1);
    }
    else
        sprintf (SizeString3, "Overall to target");


    ReturnSizeString ( (SIZE_T) (size_bytes / TimeInSeconds), SizeString1, sizeof (SizeString1) );

    dbg (ThisLogType, "%s %6.3f seconds (%sps)", SizeString3, TimeInSeconds, SizeString1);

    if (showpercentagecomplete && TotalTransferSize > 0)
    {
        PercentageBuildLoaded = (float) ( (100.0 * (double) BuildSizeTransferSoFar) / (double) TotalTransferSize);
        dbg (ThisLogType, "{percent files transferred %6.2f%%}", PercentageBuildLoaded);
    }
}

// Subtract the `struct timeval' values X and Y,
//   storing the result in RESULT.
//   Return 1 if the difference is negative, otherwise 0.
static int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
    int nsec;

    // Perform the carry for the later subtraction by updating y.
    if (x->tv_usec < y->tv_usec)
    {
        nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }

    if (x->tv_usec - y->tv_usec > 1000000)
    {
        nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    // Compute the time remaining to wait. tv_usec is certainly positive. */
    result->tv_sec  = x->tv_sec  - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    // Return 1 if result is negative.
    return x->tv_sec < y->tv_sec;
}

void parseConfigXML (SIZE_T XMLFileSize)
{
    SIZE_T PacketLoc   = 0, PacketSize = 0;

    dbg (LOG_INFO, "In parseConfigXML");

    PacketSize = strlen ( (char *) EntireXMLFileBuffer);

    while (1)
    {
        ShowXMLFileInLog = 0; // hide this from it is just config stuff
        PacketLoc = DetermineTag ( (char *) EntireXMLFileBuffer, PacketLoc, MAX_XML_FILE_SIZE); // This sets CurrentHandlerFunction()

        // This chomps up empty space till we hit a tag
        while ( IsEmptySpace ( EntireXMLFileBuffer[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= PacketSize)
            {
                break;  // we have run out of packet all is well
            }
        }

        if (PacketLoc >= PacketSize)
        {
            // we have run out of packet
            return; // all is well
        }

        if (PacketLoc == 0)
        {
            dbg (LOG_ERROR, "2. TAG not found or recognized");
            return; // FIREHOSE_ERROR;
        }

        if (ParseAttributes)
        {
            // To be this far fh.xml_buffer[PacketLoc] is pointing to the first char of the first attribute
            PacketLoc = DetermineAttributes ( (char *) EntireXMLFileBuffer, PacketLoc, MAX_XML_FILE_SIZE);

            if (PacketLoc == 0)
            {
                dbg (LOG_ERROR, "ATTRIBUTES not found or recognized");
                return; // FIREHOSE_ERROR;
            }
        }
        else
            PacketLoc = GetStringFromXML ( (char *) EntireXMLFileBuffer, PacketLoc, MAX_XML_FILE_SIZE);


        // At this point some attributes will need additional processing before
        // we call the Tag Handler function
        if (ParseComplicatedAttributes() == FIREHOSE_ERROR) // i.e. start_sector="NUM_DISKSECTORS-33."
        {
            return;
        }

        // Below is the function pointer, i.e. tag_handler_t CurrentHandlerFunction;
        CurrentHandlerFunction();

        // This chomps up empty space till we hit a tag
        while ( IsEmptySpace ( EntireXMLFileBuffer[PacketLoc] ) )
        {
            PacketLoc++;

            if (PacketLoc >= PacketSize)
            {
                break;  // we have run out of packet all is well
            }
        }

        if (EntireXMLFileBuffer[PacketLoc] == 0)
            break;  // we're out of packet, all is well

        //if(XMLFileSize-PacketLoc<50)
        //  break;
    }



    // EntireXMLFileBuffer holds the entire XML file

}

static int operatorHigherPrecedenceThan (char left, char right)
{
    char operator_string[] = "-+*/";
    char* left_pos = strchr (operator_string, left);
    char* right_pos = strchr (operator_string, right);

    if (left_pos != NULL && right_pos != NULL && left_pos > right_pos)
    {
        return 1;
    }

    return 0;
}

boolean MyParseExpression (char* buffer, SIZE_T BufferLength, SIZE_T* result)
{
    SIZE_T i = 0, TempValue = 0;
    char Operand = '+', ch;

    *result = 0;

    for (i = 0; i < BufferLength; i++)
    {
        ch = buffer[i];

        if (ch == '\0')
            break;
        else if (ch >= '0' && ch <= '9')
        {
            TempValue = (TempValue * 10) + (ch - 0x30);
        }
        else
        {
            // we have a char that is NOT A NUMBER, so evaluate
            if (Operand == '+')
            {
                *result += TempValue;
            }
            else if (Operand == '-')
            {
                *result -= TempValue;
            }

            // all others ignored

            TempValue = 0;
            Operand = '+';    // defaults to adding 0 if we get a weird operand

            if (ch == '-')
                Operand = '-';
        }
    }

    // in the event user does this "1+2+" i.e. forgets final number, it would be 0 anyway, so -0 or +0 makes no change
    if (Operand == '+')
    {
        *result += TempValue;
    }
    else if (Operand == '-')
    {
        *result -= TempValue;
    }


    return TRUE;
}

static firehose_error_t handleStorageExtras()
{
    return FIREHOSE_SUCCESS;
}


SIZE_T  ParseComplicatedAttributes (void)
{
    volatile SIZE_T i, j;

    for (i = 0; i < (SIZE_T) ReturnNumAttributes(); i++)
    {
        j = i;    // for breakpoint

        if (AllAttributes[i].Assigned == 0)
            continue; // variable wasn't even assigned, so no point checking


        if (CurrentHandlerFunction == handleStorageExtras)
        {
            // This is a special case, where the <ufs> or <emmc> tags are used and variables are sent
            // over multiple packets. In this case the struct UFS_LUN_Var is used as for temporary holding
            // and these values need to be moved to storage_extras.ufs_extras.unit[ 0 ].bLUEnable
            j = i; // for breakpoint
        }
        else if (AllAttributes[i].Type == 'i' && AllAttributes[i].Type == 'b' && AllAttributes[i].Type == 'w')
            continue; // integer attributes are not complicated, Only want 'x' or 's'


        j = i + 1;  // for breakpoint

        if (strncasecmp (AllAttributes[i].Name, "start_sector", strlen ("start_sector") ) == 0)
        {
            //printf("MATCHED - '%s'\n",AllAttributes[i].Name);

            j = i;    // for breakpoint
            /*
                            if (parseSectorValue(AllAttributes[i].Raw, sizeof(AllAttributes[i].Raw), &fh.attrs.start_sector, fh.attrs.physical_partition_number, "start_sector") == FALSE)
                            {
                                logMessage("parseSectorValue could not handle start_sector value");
                                return FIREHOSE_ERROR;
                            }
            */
            j = i;    // for breakpoint

        }  // end of looking for start_sector

// =================================================================

        if (strncasecmp (AllAttributes[i].Name, "MemoryName", strlen ("MemoryName") ) == 0)
        {
            //printf("MATCHED - '%s'\n",AllAttributes[i].Name);

            j = i;    // for breakpoint

            /*
                           // convert memory name to enum value
                           if (strncasecmp( AllAttributes[i].Raw, "emmc", strlen("eMMC") ) == 0)
                           {
                               fh.attrs.storage_type = HOTPLUG_TYPE_MMC;
                               fh.store_dev.sector_size = 512; // for eMMC

                               i = j;
                           }
                           else if (strncasecmp( AllAttributes[i].Raw, "ufs", strlen("UFS") ) == 0)
                           {
                               fh.attrs.storage_type = HOTPLUG_TYPE_UFS;
                               fh.store_dev.sector_size = 4096; // for UFS
                               i = j;
                           }
                           else
                           {
                               logMessage("Invalid storage type: %s", (char *)AllAttributes[i].Raw);
                               return FIREHOSE_ERROR;
                           }
            */
            j = i;    // for breakpoint
        }  // end of looking for MemoryName

// ==================================================================
        if (strncasecmp (AllAttributes[i].Name, "LUNum", strlen ("LUNum") ) == 0)
        {
            volatile SIZE_T  Temp1 = 0;
            volatile SIZE_T  Temp2 = 0;

            if (CurrentHandlerFunction != handleStorageExtras)
            {
                continue;
            }

            j = i;    // for breakpoint
            /*
                            Temp1 = UFS_LUN_Var_Struct.LUNum;

                            // All attributes for this LUN would have also been assigned
                            Temp2 = UFS_LUN_Var_Struct.bLUEnable;       storage_extras.ufs_extras.unit[ Temp1 ].bLUEnable           = Temp2;
                            Temp2 = UFS_LUN_Var_Struct.bBootLunID;        storage_extras.ufs_extras.unit[ Temp1 ].bBootLunID          = Temp2;
                            Temp2 = UFS_LUN_Var_Struct.bLUWriteProtect;     storage_extras.ufs_extras.unit[ Temp1 ].bLUWriteProtect     = Temp2;
                            Temp2 = UFS_LUN_Var_Struct.bMemoryType;       storage_extras.ufs_extras.unit[ Temp1 ].bMemoryType         = Temp2;
                            Temp2 = UFS_LUN_Var_Struct.bDataReliability;    storage_extras.ufs_extras.unit[ Temp1 ].bDataReliability    = Temp2;
                            Temp2 = UFS_LUN_Var_Struct.bLogicalBlockSize;   storage_extras.ufs_extras.unit[ Temp1 ].bLogicalBlockSize   = Temp2;
                            Temp2 = UFS_LUN_Var_Struct.bProvisioningType;   storage_extras.ufs_extras.unit[ Temp1 ].bProvisioningType   = Temp2;
                            Temp2 = UFS_LUN_Var_Struct.wContextCapabilities;  storage_extras.ufs_extras.unit[ Temp1 ].wContextCapabilities= Temp2;

                            // We allow user to enter size_in_KB but the real attribute is dNumAllocUnits
                            Temp2 = UFS_LUN_Var_Struct.size_in_KB;

                            storage_extras.ufs_extras.unit[ Temp1 ].dNumAllocUnits = Temp2; // for now, dNumAllocUnits *is* size_in_KB until we call ufs_commit_extras()
            */
            /*
                            if(fh.store_dev.blocks_per_alloc_unit==0)
                                OpenUFSforFWvalues(&fh.store_dev);

                            if(fh.store_dev.blocks_per_alloc_unit==0)
                            {
                                logMessage("ERROR: handleStorageExtras and ParseComplicatedAttributes() fh.store_dev.blocks_per_alloc_unit is 0. Was MemoryName='ufs' specified in the configure tag?");
                                return FIREHOSE_ERROR;
                            }

                            if (Temp2 % fh.store_dev.blocks_per_alloc_unit != 0)
                            {
                                logMessage("ERROR: For UFS, %d is not a multiple of blocks_per_alloc_unit %d",Temp2,fh.store_dev.blocks_per_alloc_unit );
                                return FIREHOSE_ERROR;
                            }

                            storage_extras.ufs_extras.unit[ Temp1 ].dNumAllocUnits = Temp2/fh.store_dev.blocks_per_alloc_unit;

                            logMessage("dNumAllocUnits=%d since size_in_KB=%d and blocks_per_alloc_unit=%d",storage_extras.ufs_extras.unit[ Temp1 ].dNumAllocUnits,Temp2,fh.store_dev.blocks_per_alloc_unit);

                            memset(&UFS_LUN_Var_Struct,0,sizeof(UFS_LUN_Var_Struct)); // clear this out for next run
            */
            j = i;    // for breakpoint

        }  // end of looking for start_sector

// =================================================================
// ==================================================================

        if (strncasecmp (AllAttributes[i].Name, "commit", strlen ("commit") ) == 0)
        {
            //printf("MATCHED - '%s'\n",AllAttributes[i].Name);

            // Need to update this value

            j = i;    // for breakpoint

            if (strncasecmp (AllAttributes[i].Raw, "true", strlen ("true") ) == 0)
                fh.attrs.commit = 1;
            else if (strncasecmp (AllAttributes[i].Raw, "1", strlen ("1") ) == 0)
                fh.attrs.commit = 1;
            else
                fh.attrs.commit = 0;

            j = i;    // for breakpoint

        }  // end of looking for start_sector

// =================================================================

    } // end for i

    return FIREHOSE_SUCCESS;

} // end SIZE_T  ParseComplicatedAttributes(void)

void ShowCommandLineOptions (void)
{
    int i, Num, PerColumn = 21, j;

    Num = sizeof (CommandLineOptions) / sizeof (CommandLineOptions[0]);

    i = 0;

    for (j = 0; j < PerColumn; j++)
    {
        if (j < Num)
            printf ("\n%-30s", CommandLineOptions[j]);

        if (1 * PerColumn + j < Num)
            printf ("\t%-30s", CommandLineOptions[1 * PerColumn + j]);

        if (2 * PerColumn + j < Num)
            printf ("\t%-30s", CommandLineOptions[2 * PerColumn + j]);

    }

    printf ("\n\n");

    i = Num;
}

int PartOfCommandLineOptions (char *sz)
{
    int j, Num, Length1, Length2;

    Num = sizeof (CommandLineOptions) / sizeof (CommandLineOptions[0]);

    Length1 = strlen (sz);

    for (j = 0; j < Num; j++)
    {
        Length2 = strlen (CommandLineOptions[j]);

        if (Length1 == Length2)
        {
            if (strncmp (sz, CommandLineOptions[j], Length2) == 0)
            {
                return 1;
            }
        } // end of length is the same
        else if (Length1 == Length2 - 1)
        {
            // that might have missed an equals sign here
            if (strncmp (sz, CommandLineOptions[j], Length1) == 0)
            {
                ShowCommandLineOptions();

                dbg (LOG_ERROR, "\n\nYou have possibly missed an '=' sign on your command line argument. "
                     "Look at the list above of possible options."
                     "\n\n\tInstead of --%s did you mean --%s=  (i.e. notice it ends in an equals sign)\n\n\n\n", sz, sz);
                ExitAndShowLog (1);
            }
        } // end of length is the same

    }

    return 0;
}

#define PARTITION_ARRAY_SECTOR      2
#define PARTITION_SIZE_IN_BYTES     128   // same for 4K
#define PARTITION_TYPE_OFFSET_IN_BYTES  0
#define PARTITION_GUID_OFFSET_IN_BYTES  16
#define PARTITION_FLBA_OFFSET_IN_BYTES  32
#define PARTITION_LLBA_OFFSET_IN_BYTES  40
#define PARTITION_NAME_OFFSET_IN_BYTES  56

void FindPartitionByLabel (SIZE_T LUN, char *LabelToMatch, char *Filename)
{
    FILE *fTemp;
    SIZE_T FileSize, NumberOfPartitionsFound = 0, offset, k, PartitionNumber, i, j, NumSectors = 0;
    SIZE_T nFirstLBA = 0, nLastLBA = 0;
    char CurrentLabel[128], CurrentUTF16Char, GUID[128], SmallString[4], LastLBA[128], FirstLBA[128];

    char szLogMessages[MAX_STRING_SIZE];
    char *sz = find_file (Filename, 1), Matched = 0;

    if (sz == NULL)
        ExitAndShowLog (1);

    fTemp = ReturnFileHandle ( sz, MAX_PATH_SIZE, "rb");    // will exit if not successful
    FileSize = ReturnFileSize (fTemp);

    BytesRead = fread (&tx_buffer, sizeof (char), FileSize, fTemp);

    if (strlen (LabelToMatch) > 0) // user might just want to list partitions, thus NO LabelToMatch
        dbg (LOG_INFO, "\n\nLooking at Partition Names, looking for '%s'", LabelToMatch);


    offset = SectorSizeInBytes;
    memset (CurrentLabel, 0x0, sizeof (CurrentLabel) );

    for (k = 0; k < 8; k++)
    {
        CurrentUTF16Char = tx_buffer[offset + k]; // don't forget, moving by 2's since UTF16, converting to UTF8
        CurrentLabel[k] = CurrentUTF16Char;
    }

    if ( strncmp (CurrentLabel, "EFI PART", MAX (strlen (CurrentLabel), strlen ("EFI PART") ) ) != 0 )
    {
        dbg (LOG_ERROR, "File '%s' does not have a GPT partition table present", Filename);
        ExitAndShowLog (1);
    }

    PartitionNumber = 0;

    for (i = 0; i < 128; i++)
    {
        offset = PARTITION_ARRAY_SECTOR * SectorSizeInBytes;
        offset += i * PARTITION_SIZE_IN_BYTES;
        offset += PARTITION_NAME_OFFSET_IN_BYTES;

        memset (CurrentLabel, 0x0, sizeof (CurrentLabel) );

        for (k = 0; k < 72; k += 2)
        {
            CurrentUTF16Char = tx_buffer[offset + k]; // don't forget, moving by 2's since UTF16, converting to UTF8

            if (CurrentUTF16Char > '\x7E' || CurrentUTF16Char < '\x20')
                break;

            CurrentLabel[k / 2] += CurrentUTF16Char;
        }


        if (strlen (CurrentLabel) == 0)
        {
            break;
        }

        offset = PARTITION_ARRAY_SECTOR * SectorSizeInBytes;
        offset += i * PARTITION_SIZE_IN_BYTES;
        offset += PARTITION_GUID_OFFSET_IN_BYTES;
        memset (GUID, 0x0, sizeof (GUID) );
        k = 15;

        for (j = 0; j < 16; j++)
        {
            CurrentUTF16Char = tx_buffer[offset + k]; // don't forget, moving by 2's since UTF16, converting to UTF8
            sprintf (SmallString, "%.2X", CurrentUTF16Char & 0xFF);

            GUID[2 * j]   = SmallString[0];
            GUID[2 * j + 1] = SmallString[1];

            k--;
        }


        offset = PARTITION_ARRAY_SECTOR * SectorSizeInBytes;
        offset += i * PARTITION_SIZE_IN_BYTES;
        offset += PARTITION_LLBA_OFFSET_IN_BYTES;
        memset (LastLBA, 0x0, sizeof (LastLBA) );
        k = 7;

        for (j = 0; j < 8; j++)
        {
            CurrentUTF16Char = tx_buffer[offset + k]; // don't forget, moving by 2's since UTF16, converting to UTF8
            sprintf (SmallString, "%.2X", CurrentUTF16Char & 0xFF);

            LastLBA[2 * j]   = SmallString[0];
            LastLBA[2 * j + 1] = SmallString[1];

            k--;
        }


        offset = PARTITION_ARRAY_SECTOR * SectorSizeInBytes;
        offset += i * PARTITION_SIZE_IN_BYTES;
        offset += PARTITION_FLBA_OFFSET_IN_BYTES;
        memset (FirstLBA, 0x0, sizeof (FirstLBA) );
        k = 7;

        for (j = 0; j < 8; j++)
        {
            CurrentUTF16Char = tx_buffer[offset + k]; // don't forget, moving by 2's since UTF16, converting to UTF8
            sprintf (SmallString, "%.2X", CurrentUTF16Char & 0xFF);

            FirstLBA[2 * j]   = SmallString[0];
            FirstLBA[2 * j + 1] = SmallString[1];

            k--;
        }

        i = i;

        for (j = 0; j < strlen (FirstLBA); j++)
        {
            nFirstLBA = nFirstLBA * 16;

            if (FirstLBA[j] >= 0x30 && FirstLBA[j] <= 0x39)
                nFirstLBA += (FirstLBA[j] - 0x30);
            else
                nFirstLBA += (FirstLBA[j] - 55); // 'A' = 65, and 0xA is 10, therefore subtrace (65-10)=55
        }

        for (j = 0; j < strlen (LastLBA); j++)
        {
            nLastLBA = nLastLBA * 16;

            if (LastLBA[j] >= 0x30 && LastLBA[j] <= 0x39)
                nLastLBA += (LastLBA[j] - 0x30);
            else
                nLastLBA += (LastLBA[j] - 55); // 'A' = 65, and 0xA is 10, therefore subtrace (65-10)=55
        }

        NumSectors = nLastLBA - nFirstLBA + 1;

        // to be here we have the label
        Matched = 0;

        if (strlen (LabelToMatch) > 0)
        {
            if ( strncasecmp (CurrentLabel, LabelToMatch, MAX (strlen (CurrentLabel), strlen (LabelToMatch) ) ) == 0 )
            {
                Matched = 1;
            }
        }

        if (strlen (CurrentLabel) > 0)
        {
            char FileSize1[2048];
            ReturnSizeString (NumSectors * SectorSizeInBytes, FileSize1, sizeof (FileSize1) );

            NumberOfPartitionsFound += 1;

            if (IsASCIIString (CurrentLabel) )
            {
                sprintf (temp_buffer, "\n(%.2d) '%36s' - %10d sectors (%12s) - at sector %10d", (int) PartitionNumber, CurrentLabel, (unsigned int) NumSectors, FileSize1, (unsigned int) nFirstLBA);
                AppendToBuffer (szLogMessages, temp_buffer, MAX_STRING_SIZE);
            }
            else
            {
                sprintf (temp_buffer, "\n(%.2d) Found Partition Name: only garbage characters found", (unsigned int) PartitionNumber );
                AppendToBuffer (szLogMessages, temp_buffer, MAX_STRING_SIZE);
            }
        }

        printf ("%s", temp_buffer);

        PartitionNumber += 1;

    } // end for i




    fclose (fTemp);
    fTemp = NULL;

}

int IsASCIIString (char *sz)
{
    int i, j;

    j = strlen (sz);

    for (i = 0; i < j; i++)
    {
        if ( (unsigned) sz[i] > 128 || sz[i] < 20)
            return 0; // not all ASCII characters
    }

    return 1; // to be here means they are all ASCII characters
}


void ReturnSizeString (unsigned long long size_bytes, char *sz, unsigned long long Length)
{
    //static char MyReturnSizeString[2048];

    if (size_bytes >= (1024 * 1024 * 1024) )
        sprintf (sz, "%.2f GB", (size_bytes * 1.0) / (1024.0 * 1024.0 * 1024.0) );
    else if (size_bytes >= (1024 * 1024) )
        sprintf (sz, "%.2f MB", (size_bytes * 1.0) / (1024.0 * 1024.0) );
    else if (size_bytes >= (1024) )
        sprintf (sz, "%.2f KB", (size_bytes * 1.0) / (1024.0) );
    else
        sprintf (sz, "%.2f B", (size_bytes * 1.0) );

    //return MyReturnSizeString;
}

void ParseContentsXML (char *FileAndPath)
{
    // FileAndPath = "\\\\sundae\\builds664\\PROD\\M8994AAAAANLYD1024.4\\contents.xml"

    FILE *fx;
    char *pch, *pchMAX, *pchOld;
    SIZE_T CurrentPacketLoc = 0, i, j, k, StringLength;
    SIZE_T FileSize;
    char StorageType[] = "emmc", SaveThis = 0; // 1=eMMC, 2=UFS, 3=NAND
    int result;

    if (FileAndPath == '\0' || strlen (FileAndPath) == 0)
        return;

    dbg (LOG_INFO, "Attempting to access '%s'", FileAndPath);
    fx = ReturnFileHandle (FileAndPath, MAX_PATH_SIZE, "r");    // will exit if not successful
    FileSize = ReturnFileSize (fx);

    dbg (LOG_DEBUG, "CONTENTS.xml file size is %"SIZE_T_FORMAT" bytes", FileSize);

    if (FileSize == 0)
    {
        dbg (LOG_INFO, "--contentsxml file size was 0 bytes!!");
        ExitAndShowLog (1);
    }

    if (FileSize > sizeof (EntireXMLFileBuffer) )
    {
        dbg (LOG_ERROR, "The contents.xml you provided is %i bytes and is bigger than %"SIZE_T_FORMAT" bytes", FileSize, sizeof (EntireXMLFileBuffer) );
        ExitAndShowLog (1);
    }

    memset (EntireXMLFileBuffer, 0, sizeof (EntireXMLFileBuffer) );
    BytesRead = fread (EntireXMLFileBuffer, 1, FileSize, fx);
    fclose (fx);
    fx = NULL;

    FileSize = BytesRead;




    CurrentPacketLoc = 0; // back to the start

    while (1)
    {
        // TODO Need the Linux path if not windows
        pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "<windows_root_path");         // Find a space after the TAG name

        if (pch == NULL) //'\0')   // if null, XML is not formed correctly
        {
            // We're done, no more files
            break;
        }

        pch += strlen ("<windows_root_path"); // here pch = flavor="asic"> OR pch = >

        CurrentPacketLoc = pch - EntireXMLFileBuffer;

        // Now hunt for > character
        for (i = CurrentPacketLoc; i < FileSize; i++)
        {
            if ( EntireXMLFileBuffer[i] == '>' )
                break;
        }

        CurrentPacketLoc = i + 1;

        pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "</windows_root_path>");

        if (pch == NULL) //'\0')   // if null, XML is not formed correctly
        {
            // We're done, no more files
            ExitAndShowLog (1);
        }

        if ( CopyString (ContensXMLPath[NumContentsXMLPath].Path, EntireXMLFileBuffer, 0, CurrentPacketLoc, pch - &EntireXMLFileBuffer[CurrentPacketLoc], sizeof (ContensXMLPath[NumContentsXMLPath].Path), MAX_XML_FILE_SIZE) == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.start_sector", "0", strlen ("0") );
            ExitAndShowLog (1);
        }

        ContensXMLPath[NumContentsXMLPath].Address = CurrentPacketLoc;


        // SPECIAL CASE, if the path is ./ then I want to extract the path from FileAndPath
        if (ContensXMLPath[NumContentsXMLPath].Path[0] == '.')
        {
            for (i = strlen (FileAndPath); i > 1; i--)
            {
                if (FileAndPath[i] == '/' || FileAndPath[i] == '\\')
                    break;
            }

            if (i > 1) // possible that FileAndPath="contents.xml", and thus doesn't have a directory path in front of it
            {
                if ( CopyString (ContensXMLPath[NumContentsXMLPath].Path, FileAndPath, 0, 0, i + 1, sizeof (ContensXMLPath[NumContentsXMLPath].Path), MAX_XML_FILE_SIZE) == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.start_sector", "0", strlen ("0") );
                    ExitAndShowLog (1);
                }
            }
        }

        NumContentsXMLPath++;

    } // end while(1)





    CurrentPacketLoc = 0; // back to the start

    while (1)
    {
        char *pch_ref;
        SaveThis = 1; // assume we will keep this. We won't keep those with * in filename

        ContensXML[NumContensXML].StorageType = 0;  // 0 is both or unknown storage type, 'e'=emmc,'u'=ufs,'n'=nand
        ContensXML[NumContensXML].FileType    = 0; // assume normal file such as sbl1.mbn
        ContensXML[NumContensXML].Flavor[0]   = 0; // assume no flavor

        // Get the filename ---------------------------------------------------------------------------------------------------------------------------------------
        // Get the filename ---------------------------------------------------------------------------------------------------------------------------------------
        // Get the filename ---------------------------------------------------------------------------------------------------------------------------------------

        //<file_name>mba.mbn</file_name>
        //<file_name flavor="asic_8916">sbl1.mbn</file_name>

        pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "<file_name"); // this gets us to either <filename> OR <file_name flavor="asic_8916">
        if (pch == NULL)
        {
            // We're done, no more files
            break;
        }

        // Check if <file_ref> includes "ignore="true""
        pch_ref = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "<file_ref"); // this gets us to either <file_ref
        // Make sure the <file_ref tag comes before the <file_name tag
        if(pch_ref != NULL && pch_ref < pch)
        {
          char *pch_end;
          pch_end = strstr (pch_ref, ">");
          pch_ref = strstr (pch_ref, "ignore=\"");
          // Check if ignore tag is inside the tag.
          if(pch_ref != NULL && pch_ref < pch_end)
          {
            pch_ref += strlen("ignore=\"");
            if (strncmp (pch_ref, "true", 4) == 0 )
            {
              // Ignore is TRUE, skip this <file_ref>
              pch_ref = strstr (pch_end, "</file_ref>");
              CurrentPacketLoc = pch_ref - EntireXMLFileBuffer;
              continue;
            }
          }
        }

        CurrentPacketLoc = pch - EntireXMLFileBuffer;

        // Need to decide

        if (EntireXMLFileBuffer[CurrentPacketLoc + strlen ("<file_name")] == '>')
        {
            // we have this <file_name>mba.mbn</file_name>
            pch += strlen ("<file_name>");
        }
        else
        {
            // we have this <file_name flavor="asic_8916">sbl1.mbn</file_name>
            //ContensXML[NumContensXML].Flavor

            pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "flavor=\""); // this gets us to either <filename> OR <file_name flavor="asic_8916">

            if (pch == NULL)
            {
                dbg (LOG_ERROR, "Looks like fh_loader cannot parse this contents.xml file. Confused by what looked like <file_name flavor=, shown below\n\n%s", &EntireXMLFileBuffer[CurrentPacketLoc]);
                ExitAndShowLog (1);
            }

            pch += strlen ("flavor=\"");
            CurrentPacketLoc = pch - EntireXMLFileBuffer; // &EntireXMLFileBuffer[CurrentPacketLoc] = rawprogram_unsparse.xml</file_name>

            // where is the closing "
            for (i = CurrentPacketLoc; i < FileSize; i++)
            {
                if (EntireXMLFileBuffer[i] == '\"')
                {
                    break;
                }
            }

            if ( CopyString (ContensXML[NumContensXML].Flavor, EntireXMLFileBuffer, 0, CurrentPacketLoc, i - CurrentPacketLoc, sizeof (ContensXML[NumContensXML].Flavor), MAX_XML_FILE_SIZE) == 0)
            {
                dbg (LOG_ERROR, "Failed to copy string into ContensXML[NumContensXML].Flavor");
                ExitAndShowLog (1);
            }

            if ( strncmp (ContensXML[NumContensXML].Flavor, flavor, MAX (strlen (ContensXML[NumContensXML].Flavor), strlen (flavor) ) ) != 0 )
            {
                // don't match, user is looking for a different flavor, ignore this
                continue;
            }

            i = i;

            pch += strlen (ContensXML[NumContensXML].Flavor) + 2; // +2 is for ">
            CurrentPacketLoc = pch - EntireXMLFileBuffer; // &EntireXMLFileBuffer[CurrentPacketLoc] = rawprogram_unsparse.xml</file_name>

        }


        // At this point &EntireXMLFileBuffer[CurrentPacketLoc] is pointing at <file_name>rawprogram_unsparse.xml</file_name>
        // but there is a chance the XML line prior to this is <partition_file storage_type="emmc"> and I want to know the
        // storage_type if it exists. So look backwards for a <, then forwards for storage type

        for (i = CurrentPacketLoc - 1; i > 1; i--)
        {
            if (EntireXMLFileBuffer[i] == '<')
            {
                break;
            }
        }


        // here pch = NON-HLOS.bin"

        CurrentPacketLoc = pch - EntireXMLFileBuffer; // &EntireXMLFileBuffer[CurrentPacketLoc] = rawprogram_unsparse.xml</file_name>

        // To be here means I should be at something like       <partition_file storage_type="emmc">
        //                                                         <file_name>rawprogram_unsparse.xml</file_name>

        pch = strstr (&EntireXMLFileBuffer[i], "<partition_file");

        if (pch != NULL) //'\0')
        {
            // we found it, but did we go to far?
            if (pch < &EntireXMLFileBuffer[CurrentPacketLoc])
            {
                // we found it *before* our current position, so this is for this <file_name>
                ContensXML[NumContensXML].FileType = 'r'; // this is like a rawprogram0.xml file
            }
        }


        pch = strstr (&EntireXMLFileBuffer[i], "<partition_patch_file");

        if (pch != NULL) //'\0')
        {
            // we found it, but did we go to far?
            if (pch < &EntireXMLFileBuffer[CurrentPacketLoc])
            {
                // we found it *before* our current position, so this is for this <file_name>
                ContensXML[NumContensXML].FileType = 'p'; // this is like a patch0.xml file
            }
        }

        // Now look forward for storage_type

        pch = strstr (&EntireXMLFileBuffer[i], "storage_type=");         // Find a space after the TAG name

        if (pch != NULL) //'\0')
        {
            // we found it, but did we go to far?
            if (pch < &EntireXMLFileBuffer[CurrentPacketLoc])
            {
                // we found it *before* our current position, so this is for this <file_name>
                pch += strlen ("storage_type=") + 1; // here pch = emmc"

                if ( pch[0] == 'e' && pch[1] == 'm' && pch[2] == 'm' && pch[3] == 'c' )
                    ContensXML[NumContensXML].StorageType = 'e';  // 'e'=emmc,'u'=ufs,'n'=nand
                else if ( pch[0] == 'u' && pch[1] == 'f' && pch[2] == 's' )
                    ContensXML[NumContensXML].StorageType = 'u';  // 'e'=emmc,'u'=ufs,'n'=nand
                else if ( pch[0] == 'n' && pch[1] == 'a' && pch[2] == 'n' && pch[3] == 'd' )
                    ContensXML[NumContensXML].StorageType = 'n';  // 'e'=emmc,'u'=ufs,'n'=nand

            } // found the attr earlier than the <file_name> tag
        } // end if(pch!=NULL) - i.e. we found a storage_type attr
        else
        {
            //dbg(LOG_DEBUG,"Contents.xml is missing storage_type= and therefore assuming both eMMC and UFS");
            //ContensXML[NumContensXML].StorageType = 'e';  // 'e'=emmc,'u'=ufs,'n'=nand
        }



        pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "</file_name>");         // Find a space after the TAG name

        if (pch == NULL) //'\0')   // if null, XML is not formed correctly
        {
            dbg (LOG_ERROR, "2) XML not formed correctly. Expected one SPACE character at loc %d", CurrentPacketLoc);
            ExitAndShowLog (1);
        }

        if ( CopyString (ContensXML[NumContensXML].Filename, EntireXMLFileBuffer, 0, CurrentPacketLoc, pch - &EntireXMLFileBuffer[CurrentPacketLoc], sizeof (ContensXML[NumContensXML].Filename), MAX_XML_FILE_SIZE) == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.start_sector", "0", strlen ("0") );
            ExitAndShowLog (1);
        }

        if ( strncmp (ContensXML[NumContensXML].Filename, "qdsp6sw.mbn", MAX (strlen (ContensXML[NumContensXML].Filename), strlen ("qdsp6sw.mbn") ) ) == 0 )
        {
            CurrentPacketLoc = CurrentPacketLoc;  // solely for breakpoint, i.e. change filename above as needed
        }


        ContensXML[NumContensXML].Address = CurrentPacketLoc;

        // if the filename has a * in it, we won't save those
        for (i = 0; i < strlen (ContensXML[NumContensXML].Filename); i++)
        {
            if ( ContensXML[NumContensXML].Filename[i] == '*' || ContensXML[NumContensXML].Filename[i] == '$')
                SaveThis = 0;
        }


        // Get the path ---------------------------------------------------------------------------------------------------------------------------------------
        // Get the path ---------------------------------------------------------------------------------------------------------------------------------------
        // Get the path ---------------------------------------------------------------------------------------------------------------------------------------

        //<file_path flavor="asic">
        //<file_path flavor="modemlite">
        //<file_path>

        pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "<file_path");

        if (pch == NULL) //'\0')   // if null, XML is not formed correctly
        {
            dbg (LOG_ERROR, "3) XML not formed correctly. Expected one SPACE character at loc %d", CurrentPacketLoc);
            ExitAndShowLog (1);
        }

        pch += strlen ("<file_path"); // here pch = flavor="asic"> OR pch = >

        CurrentPacketLoc = pch - EntireXMLFileBuffer;

        if (pch[0] == ' ')
        {
            // to be here means there are other tags in <file_path >.
            // We will be hunting for flavor possibly, so we need to know how far we can go
            pchMAX = strstr(&EntireXMLFileBuffer[CurrentPacketLoc], ">"); // End of <file_path.....>

            while (1)
            {
                pchOld = pch; // backup since the next search can blow way past the actual file

                pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "flavor="); // <file_path flavor="asic"> --> flavor="asic">

                if (pch == NULL || pch > pchMAX) // if null, XML is not formed correctly
                {
                    dbg (LOG_INFO, "Could not find the --flavor='%s' for filename '%s", flavor, ContensXML[NumContensXML].Filename);
                    pch = pchOld;
                    break;
                }

                pch += strlen ("flavor=") + 1; // here pch = asic"> timmy
                CurrentPacketLoc = pch - EntireXMLFileBuffer;

                // not guaranteed it says asic, so now find next >

                // where is the closing "
                for (i = CurrentPacketLoc; i < FileSize; i++)
                {
                    if (EntireXMLFileBuffer[i] == '\"')
                    {
                        break;
                    }
                }

                if ( CopyString (ContensXML[NumContensXML].Flavor, EntireXMLFileBuffer, 0, CurrentPacketLoc, i - CurrentPacketLoc, sizeof (ContensXML[NumContensXML].Flavor), MAX_XML_FILE_SIZE) == 0)
                {
                    dbg (LOG_ERROR, "Failed to copy string into ContensXML[NumContensXML].Flavor");
                    ExitAndShowLog (1);
                }

                if (strlen (flavor) == 0)
                    break;  // user didn't specify a flavor, so we're good with the first one

                // to be this far user specified the flavor
                if ( strncmp (ContensXML[NumContensXML].Flavor, flavor, MAX (strlen (ContensXML[NumContensXML].Flavor), strlen (flavor) ) ) != 0 )
                {
                    // don't match, user is looking for a different flavor, ignore this
                    continue;
                }

                break;  // we are done

            } // end while(1)

            pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], ">"); // <file_path flavor="asic"> -->   >

            if (pch == NULL) //'\0')   // if null, XML is not formed correctly
            {
                dbg (LOG_ERROR, "5) XML not formed correctly. Expected one SPACE character at loc %d", CurrentPacketLoc);
                ExitAndShowLog (1);
            }

            if (pch > pchMAX)
            {
                dbg (LOG_ERROR, "Could not find the --flavor='%s' for filename '%s'", flavor, ContensXML[NumContensXML].Filename);
                ExitAndShowLog (1);
            }

            pch += strlen (">");

        }
        else
        {
            //pch[0]='>'
            pch++;
        }

        CurrentPacketLoc = pch - EntireXMLFileBuffer;

        pch = strstr (&EntireXMLFileBuffer[CurrentPacketLoc], "</file_path>");

        if (pch == NULL) //'\0')   // if null, XML is not formed correctly
        {
            dbg (LOG_ERROR, "6) XML not formed correctly. Expected one SPACE character at loc %d", CurrentPacketLoc);
            ExitAndShowLog (1);
        }


        // temp buffer
        if ( CopyString (temp_buffer, EntireXMLFileBuffer, 0, CurrentPacketLoc, pch - &EntireXMLFileBuffer[CurrentPacketLoc], sizeof (temp_buffer), sizeof (EntireXMLFileBuffer) ) == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.start_sector", "0", strlen ("0") );
            ExitAndShowLog (1);
        }

        for (j = 0; j < NumContentsXMLPath; j++)
        {
            if (ContensXMLPath[j].Address > ContensXML[NumContensXML].Address)
            {
                break;
            } // end of comparing address

        } // end j

        if ( CopyString (ContensXML[NumContensXML].Path, ContensXMLPath[j - 1].Path, 0, 0, strlen (ContensXMLPath[j - 1].Path), MAX_PATH_SIZE, MAX_PATH_SIZE) == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.start_sector", "0", strlen ("0") );
            ExitAndShowLog (1);
        }

        if ( CopyString (ContensXML[NumContensXML].Path, temp_buffer, strlen (ContensXML[NumContensXML].Path), 0, strlen (temp_buffer), MAX_PATH_SIZE, MAX_PATH_SIZE) == 0)
        {
            dbg (LOG_ERROR, "Failed to copy '%s' of length %"SIZE_T_FORMAT" bytes into fh.attrs.start_sector", "0", strlen ("0") );
            ExitAndShowLog (1);
        }


        // if the path has a * in it, we won't save those
        for (i = 0; i < strlen (ContensXML[NumContensXML].Path); i++)
        {
            if ( ContensXML[NumContensXML].Path[i] == '*' )
                SaveThis = 0;
        }

        // There is a chance the path is in this format ${modem_bid:FAAAANAZ}
        // or worse <file_path>debug_image/build/ms/bin/${sdi_bid:AAAAANAZ}/msm8994/</file_path>
        //<file_path>debug_image/build/ms/bin/          AAAAANAZ /msm8994/</file_path>

        if (SaveThis)
        {
            j = 0;
            k = 0;

            for (i = 0; i < strlen (ContensXML[NumContensXML].Path); i++)
            {
                if ( ContensXML[NumContensXML].Path[i] == '$')
                {
                    j = i;
                }

                if ( ContensXML[NumContensXML].Path[i] == ':')
                {
                    k = i + 1;
                    break;
                }
            }

            // Don't correct this though \\corebsp-tst-112\c$\preflight\

            if ( ContensXML[NumContensXML].Path[j + 1] == '\\')
            {
                j = 0; // we have \\corebsp-tst-112\c$\preflight\, so don't correct it
            }

            if (j > 0)
            {
                StringLength = strlen (ContensXML[NumContensXML].Path);

                for (i = j; i < StringLength; i++)
                {
                    if (ContensXML[NumContensXML].Path[k] == '}')
                    {
                        k++;
                        ContensXML[NumContensXML].Path[i] = SLASH;

                        if (ContensXML[NumContensXML].Path[k] == 0)
                            ContensXML[NumContensXML].Path[i + 1] = '\0';
                        else
                            k++;  // get past the slash // we have this case <file_path>debug_image/build/ms/bin/${sdi_bid:AAAAANAZ}/msm8994/</file_path>

                        //break;
                    }
                    else
                    {
                        ContensXML[NumContensXML].Path[i] = ContensXML[NumContensXML].Path[k];
                        k++;
                    }
                } // end for i
            } // end if(j>0), we found the $ format

            // Before saving this file, let's make sure the user doesn't really want it
            NumContensXML++;  // those with a * in the filename are not saved

            if (num_filter_not_files > 0)
            {
                if (ThisFileIsInNotFilterFiles (ContensXML[NumContensXML - 1].Filename) )
                {
                    dbg (LOG_INFO, "'%s' is being SKIPPED since it was in --notfiles provided by the user at the command line", ContensXML[NumContensXML - 1].Filename);
                    NumContensXML--;  // back out the above change
                }
            } // end of if(num_filter_not_files>0)
        }

        if (NumContensXML >= MAX_CONTENTS_XML_ENTRIES)
        {
            dbg (LOG_ERROR, "More than %i entries in contents.xml, can't continue", MAX_CONTENTS_XML_ENTRIES);
            ExitAndShowLog (1);
        }

    } // end while(1)



    // now clean up the slashes

    for (i = 0; i < NumContensXML; i++)
    {
        for (j = 0; j < strlen (ContensXML[i].Path); j++)
        {
            if (ContensXML[i].Path[j] == WRONGSLASH)
                ContensXML[i].Path[j] = SLASH;
        }

        // does it end in a slash?
        if (ContensXML[i].Path[j - 1] != SLASH)
        {
            ContensXML[i].Path[j]   = SLASH;
            ContensXML[i].Path[j + 1] = '\0';
        }

        i = i;
    }



    dbg (LOG_INFO, "\n\nEMMC\n");

    j = 0;

    for (i = 0; i < NumContensXML; i++)
    {
        if ( ContensXML[i].StorageType == 'e' )
        {
            j++;
            //dbg(LOG_INFO,"(%2i) %i %30s    %s",j,ContensXML[i].FileType,ContensXML[i].Filename,ContensXML[i].Path);
            dbg (LOG_INFO, "(%2i) %30s    %s", j, ContensXML[i].Filename, ContensXML[i].Path);
        } // end if( ContensXML[i].StorageType == 'e' )

    } // end for(i=0;i<NumContensXML;i++)

    dbg (LOG_INFO, "\n\nUFS\n");
    j = 0;

    for (i = 0; i < NumContensXML; i++)
    {
        if ( ContensXML[i].StorageType == 'u' )
        {
            j++;
            //dbg(LOG_INFO,"(%2i) %i %30s    %s",j,ContensXML[i].FileType,ContensXML[i].Filename,ContensXML[i].Path);
            dbg (LOG_INFO, "(%2i) %30s    %s", j, ContensXML[i].Filename, ContensXML[i].Path);

        }

    }


    dbg (LOG_INFO, "\n\nBOTH\n");
    j = 0;
    {
        char *pch;

        for (i = 0; i < NumContensXML; i++)
        {
            pch = strstr (ContensXML[i].Filename, "prog_emmc");

            if (pch != NULL)
                DeviceProgrammerIndex = i;

            if ( ContensXML[i].StorageType == 0 )
            {
                j++;
                //dbg(LOG_INFO,"(%2i) %i %36s  %s",j,ContensXML[i].FileType,ContensXML[i].Filename,ContensXML[i].Path);
                dbg (LOG_INFO, "(%2i) %36s  %s", j, ContensXML[i].Filename, ContensXML[i].Path);

            }
        }
    }


    // if flattening the build, need to copy the XML files also

    if (FlattenBuild)
    {
        // to be here we are just copying the file to a local directory, and not actually doing the <program>

        // user provided this path, therefore save files into it
        memset (flattenbuildto, 0x0, sizeof (flattenbuildto) );

        if ( CopyString (flattenbuildto, flattenbuildvariant, 0, 0, strlen (flattenbuildvariant), sizeof (flattenbuildto), sizeof (flattenbuildvariant) )  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildto", flattenbuildvariant, strlen (flattenbuildvariant) );
            ExitAndShowLog (1);
        }

        if ( CopyString (flattenbuildto, fh.attrs.filename, strlen (flattenbuildto), 0, strlen (fh.attrs.filename), sizeof (flattenbuildto), strlen (fh.attrs.filename) )  == 0)
        {
            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildto", fh.attrs.filename, strlen (fh.attrs.filename) );
            ExitAndShowLog (1);
        }


        dbg (LOG_INFO, "\n\nCopy XML files\n");
        j = 0;
        {
            char *pch, *pch2;

            for (i = 0; i < NumContensXML; i++)
            {
                pch = strstr (ContensXML[i].Filename, ".xml");
                pch2 = strstr (ContensXML[i].Filename, "firehose");

                if (pch != NULL || pch2 != NULL)
                {
                    // copy this file it's an XML file

                    if (fh.attrs.MemoryName[0] == ContensXML[i].StorageType || ContensXML[i].StorageType == 0 )
                    {
                        dbg (LOG_INFO, "(%2i) %36s  %s", j, ContensXML[i].Filename, ContensXML[i].Path);

                        if ( CopyString (tx_buffer, ContensXML[i].Path, 0, 0, strlen (ContensXML[i].Path), sizeof (tx_buffer), sizeof (ContensXML[i].Path) )  == 0)
                        {
                            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into tx_buffer", ContensXML[i].Path, strlen (ContensXML[i].Path) );
                            ExitAndShowLog (1);
                        }

                        if ( CopyString (tx_buffer, ContensXML[i].Filename, strlen (tx_buffer), 0, strlen (ContensXML[i].Filename), sizeof (tx_buffer), strlen (ContensXML[i].Filename) )  == 0)
                        {
                            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into tx_buffer", ContensXML[i].Filename, strlen (ContensXML[i].Filename) );
                            ExitAndShowLog (1);
                        }


                        // user provided this path, therefore save files into it
                        memset (flattenbuildto, 0x0, sizeof (flattenbuildto) );

                        if ( CopyString (flattenbuildto, flattenbuildvariant, 0, 0, strlen (flattenbuildvariant), sizeof (flattenbuildto), sizeof (flattenbuildvariant) )  == 0)
                        {
                            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildto", flattenbuildvariant, strlen (flattenbuildvariant) );
                            ExitAndShowLog (1);
                        }

                        if ( CopyString (flattenbuildto, ContensXML[i].Filename, strlen (flattenbuildto), 0, strlen (ContensXML[i].Filename), sizeof (flattenbuildto), strlen (ContensXML[i].Filename) )  == 0)
                        {
                            dbg (LOG_ERROR, "Failed to copy string '%s' of length %"SIZE_T_FORMAT" bytes into flattenbuildto", ContensXML[i].Filename, strlen (ContensXML[i].Filename) );
                            ExitAndShowLog (1);
                        }

                        result = MyCopyFile(tx_buffer, flattenbuildto);
                        if (result == -1) // this is only copying XML files like rawprogram0.xml and patch0.xml
                        {
                            dbg (LOG_ERROR, "Failed to copy '%s'\n\t\t\tto '%s'\n\n", tx_buffer, flattenbuildto);
                            ExitAndShowLog (1);
                        }
                        else if (result == 0)
                            dbg (LOG_INFO, "Copied '%s'\n\t\t\tto '%s'\n\n", tx_buffer, flattenbuildto);

                    } // emd of checking storage type
                } // end if(pch!=NULL)
            } // end for i
        }
    }





    NumContensXML = NumContensXML;

}

int MyCopyFile (char *FileNameSource, char *FileNameDest)
{
    FILE *f1;
    FILE *f2;
    //char FileBuffer[1024*1024]; // char temp_buffer[FIREHOSE_TX_BUFFER_SIZE];
    SIZE_T BytesRead = 0, BytesWritten = 0;
    struct _stat64 buf;

    if (!ForceOverwrite)
    {
      if (_stat64(FileNameDest, &buf) == 0) //check if file already exists
      {
        dbg(LOG_INFO, "%s already exists and --forceoverwrite not set. Leaving file as is.", FileNameDest);
        return 1;
      }
    }

    f1 = ReturnFileHandle (FileNameSource, MAX_PATH_SIZE, "rb");    // will exit if not successful
    f2 = ReturnFileHandle (FileNameDest, MAX_PATH_SIZE, "wb");    // will exit if not successful

    printf (" **BEGIN COPY** ");

    while (1)
    {
        BytesWritten = 0; // reset

        BytesRead = fread (temp_buffer, sizeof (char), sizeof (temp_buffer), f1);

        printf (".");

        if (BytesRead == 0)
            break;

        BytesWritten = fwrite (temp_buffer, sizeof (char), BytesRead, f2);

        if (BytesWritten != BytesRead)
            break;

    } // end while

    fclose (f1);
    f1 = NULL;
    fclose (f2);
    f2 = NULL;

    if (BytesWritten != BytesRead)
        return -1;
    else
        return 0;
}



void SortMyXmlFiles (void)
{
    // configure first
    // erase
    // EVERYTHING ELSE
    // patch
    // reset

    SIZE_T k = 0, j = 0;
    char *pch, *pch2;


    // Make a backup
    while (XMLStringTable[k][0] != '\0')
    {
        CopyString (XMLStringTableTemp[k], XMLStringTable[k], 0, 0, strlen (XMLStringTable[k]), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
        k++;
    } // end while

    k = 0;

    while (XMLStringTableTemp[k][0] != '\0')
    {

        pch = strstr (XMLStringTableTemp[k], "<configure ");

        if (pch != NULL)
        {
            CopyString (XMLStringTable[j], XMLStringTableTemp[k], 0, 0, strlen (XMLStringTableTemp[k]), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            XMLStringTableTemp[k][0] = '*'; // mark it as used up
            j++;
        }

        k++;
    } // end while

    k = 0;

    while (XMLStringTableTemp[k][0] != '\0')
    {

        pch = strstr (XMLStringTableTemp[k], "<erase ");

        if (pch != NULL)
        {
            CopyString (XMLStringTable[j], XMLStringTableTemp[k], 0, 0, strlen (XMLStringTableTemp[k]), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            XMLStringTableTemp[k][0] = '*'; // mark it as used up
            j++;
        }

        k++;
    } // end while


    k = 0;

    while (XMLStringTableTemp[k][0] != '\0')
    {
        pch = strstr (XMLStringTableTemp[k], "<patch ");
        pch2 = strstr (XMLStringTableTemp[k], "<power ");

        if (pch == NULL && pch2 == NULL)
        {
            // to be here it's not <patch or <power
            if (XMLStringTableTemp[k][0] != '*')
            {
                CopyString (XMLStringTable[j], XMLStringTableTemp[k], 0, 0, strlen (XMLStringTableTemp[k]), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
                XMLStringTableTemp[k][0] = '*'; // mark it as used up
                j++;
            }
        }

        k++;
    } // end while


    k = 0;

    while (XMLStringTableTemp[k][0] != '\0')
    {
        pch = strstr (XMLStringTableTemp[k], "<patch ");

        if (pch != NULL)
        {
            CopyString (XMLStringTable[j], XMLStringTableTemp[k], 0, 0, strlen (XMLStringTableTemp[k]), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            XMLStringTableTemp[k][0] = '*'; // mark it as used up
            j++;
        }

        k++;
    } // end while

    k = 0;

    while (XMLStringTableTemp[k][0] != '\0')
    {
        pch = strstr (XMLStringTableTemp[k], "<power ");

        if (pch != NULL)
        {
            CopyString (XMLStringTable[j], XMLStringTableTemp[k], 0, 0, strlen (XMLStringTableTemp[k]), MAX_XML_SIZE, MAX_XML_FILE_SIZE);
            XMLStringTableTemp[k][0] = '*'; // mark it as used up
            j++;
        }

        k++;
    } // end while

    k = 0; // for breakpoint

} // end of SortMyXmlFiles

void ModifyTags (void)
{
    SIZE_T k = 0, TempLength = 0;
    char *pch;

    if (!ConvertProgram2Read)
        return; // nothing to do

    // to be this far, user wants all <program tags to be converted to <read tags

    // loop through
    while (XMLStringTable[k][0] != '\0')
    {
        // backup into tx_buffer
        CopyString (tx_buffer, XMLStringTable[k], 0, 0, strlen (XMLStringTable[k]), FIREHOSE_TX_BUFFER_SIZE, MAX_XML_FILE_SIZE);

        // is there even a program tag to begin with
        pch = strstr (tx_buffer, "<program" );

        if (pch == NULL)
        {
            k++;
            continue; // not a program tag, so get out of here
        }

        memset (temp_buffer, 0x0, FIREHOSE_TX_BUFFER_SIZE); // zero out to begin

        TempLength = pch - tx_buffer;

        CopyString (temp_buffer, tx_buffer, 0, 0, TempLength, FIREHOSE_TX_BUFFER_SIZE, FIREHOSE_TX_BUFFER_SIZE);

        AppendToBuffer (temp_buffer, "<read ", FIREHOSE_TX_BUFFER_SIZE);

        TempLength += strlen ("<program "); // need to know where this ends

        AppendToBuffer (temp_buffer, &tx_buffer[TempLength], FIREHOSE_TX_BUFFER_SIZE);

        memscpy (XMLStringTable[k], MAX_XML_FILE_SIZE, temp_buffer, strlen (temp_buffer) ); // memcpy

        k++;

    } // end while

}

static firehose_error_t handleBenchmark()
{
    struct timeval time_start, time_end, AbsoluteTimeStart;
    SIZE_T FileSizeNumSectors = 0, FileSizeNumSectorsLeft = 0, NumSectors = 0, WriteCount = 0;
    SIZE_T BitFlipsDetected = 0, j, k, SizeOfDataFedToHashRoutine = 0;
    char DDRCount = 0;
    FILE *fr;
    char *pch;

    dbg (LOG_INFO, "In handleBenchmark");

    NumTries = 1001;  // this alone will make ReadPort add a sleep(1) between calls

    // tx_buffer already holds the XML file
    sendTransmitBuffer();

    if (Simulate)
    {
        InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
        AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "<response value=\"ACK\" ", MAX_READ_BUFFER_SIZE);
        AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
        CharsInBuffer = strlen (ReadBuffer);
    }

    // NOTE: The way benchmarks work, we usally poll until we get the ACK


    pch = strstr (tx_buffer, "TestDDRValidity");

    if (pch != NULL)
    {
        RawMode = 1;

        // to be this far we are in RAW MODE. Need to read the file
        fr = ReturnFileHandle ("ddr_file.bin", MAX_PATH_SIZE, "wb"); // will exit if not successful

        dbg (LOG_ALWAYS, "TestDDRValidity - Trials will appear doubled due to having 2 USB channels");


        gettimeofday (&time_start, NULL);
        AbsoluteTimeStart = time_start;

        for (k = 0; k < 2 * fh.attrs.trials; k++) // 2 times since 2 channels
        {
            FileSizeNumSectorsLeft = fh.attrs.MaxPayloadSizeToTargetInBytesSupported / fh.attrs.SECTOR_SIZE_IN_BYTES;

            sechsharm_sha256_init (&context); // init

            while (RawMode > 0)
            {
                if (MaxBytesToReadFromUSB > FileSizeNumSectorsLeft * fh.attrs.SECTOR_SIZE_IN_BYTES)
                    MaxBytesToReadFromUSB = FileSizeNumSectorsLeft * fh.attrs.SECTOR_SIZE_IN_BYTES;

                if (MaxBytesToReadFromUSB > MAX_READ_BUFFER_SIZE)
                    MaxBytesToReadFromUSB = MAX_READ_BUFFER_SIZE;

                if (Simulate)
                    CharsInBuffer = MaxBytesToReadFromUSB;

                dbg (LOG_DEBUG, "FileSizeNumSectorsLeft = %"SIZE_T_FORMAT, FileSizeNumSectorsLeft);

                GetNextPacket();  // In RawMode this will return without setting any attributes

                NumSectors        = CharsInBuffer / fh.attrs.SECTOR_SIZE_IN_BYTES;

                if (NumSectors == 0)
                {
                    dbg (LOG_ERROR, "In HandleRead ** NOTHING READ FROM TARGET ** Can't continue if not getting data. CharsInBuffer=%i and fh.attrs.SECTOR_SIZE_IN_BYTES=%"SIZE_T_FORMAT, CharsInBuffer, fh.attrs.SECTOR_SIZE_IN_BYTES);
                    PRETTYPRINT ( (unsigned char*) ReadBuffer, PacketLoc + CharsInBuffer, MAX_READ_BUFFER_SIZE); // always show everything
                    ExitAndShowLog (1);
                }

                // NOTE: On the last RAW packet there is a chance we have XML data in there too
                if (NumSectors > FileSizeNumSectorsLeft)
                    NumSectors = FileSizeNumSectorsLeft;  // this is what was left of RawData

                WriteCount++;
                fwrite (ReadBuffer, NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES, 1, fr);

                SizeOfDataFedToHashRoutine += NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES;
                sechsharm_sha256_update (&context, context.leftover, & (context.leftover_size), (unsigned char *) ReadBuffer, NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES);

                for (j = 0; j < NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES; j++)
                {
                    if (ReadBuffer[j] != (char) ( (DDRCount + (k >> 1) ) & 0xFF) )
                    {
                        dbg (LOG_ERROR, "Corruption detected. Found 0x%.2X but expected 0x%.2X", ReadBuffer[j], (unsigned char) ( (DDRCount + (k >> 1) ) & 0xFF) );
                        BitFlipsDetected++;
                    }

                    DDRCount++;
                }

                //dbg(LOG_DEBUG,"Just called fwrite with %"SIZE_T_FORMAT" CharsInBuffer=%ld\n",WriteCount,CharsInBuffer);

                gettimeofday (&time_end, NULL);


                if (NumSectors < FileSizeNumSectorsLeft)    // avoiding a roll over
                    FileSizeNumSectorsLeft  -= NumSectors;
                else
                    FileSizeNumSectorsLeft   = 0;

                if (FileSizeNumSectorsLeft <= 0)
                    RawMode = 0;

                //printf("\n\nNEW FileSizeNumSectorsLeft=%"SIZE_T_FORMAT"\n",FileSizeNumSectorsLeft);

                PacketLoc     += NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES;
                CharsInBuffer -= NumSectors * fh.attrs.SECTOR_SIZE_IN_BYTES;

                if (CharsInBuffer <= 0)
                {
                    CharsInBuffer     = 0;
                    PacketLoc       = 0;
                }


            } // end of while(RawMode>0)

            RawMode = 1;  // still alive for num trials

            gettimeofday (&time_end, NULL);

            sechsharm_sha256_final (&context, context.leftover, & (context.leftover_size), last_hash_value);
            PrettyPrintHexValueIntoTempBuffer (last_hash_value, 32, 0, 32); // from, size, offset, maxlength
            dbg (LOG_INFO, "%"SIZE_T_FORMAT" of %"SIZE_T_FORMAT" verify_programming SHA256 (%7d bytes) %s\n", k + 1, 2 * fh.attrs.trials, SizeOfDataFedToHashRoutine, temp_buffer);
            memscpy (verify_hash_value, sizeof (verify_hash_value), last_hash_value, sizeof (last_hash_value) ); // memcpy
            SizeOfDataFedToHashRoutine = 0; // reset for next run

        } // end k

        RawMode = 0;

        fclose (fr);
        fr = NULL;

        if (BitFlipsDetected)
        {
            dbg (LOG_ERROR, "%"SIZE_T_FORMAT" corrupt bytes detected. Either USB is corrupting the data, or the memory is bad, i.e. DDR bit flips??", BitFlipsDetected);
        }
        else
        {
            dbg (LOG_ALWAYS, "Your DDR and USB seem to be working great!! No corruption detected.");
            dbg (LOG_ALWAYS, "Your DDR and USB seem to be working great!! No corruption detected.");
            dbg (LOG_ALWAYS, "Your DDR and USB seem to be working great!! No corruption detected.");
        }

        // Restored this to normal
        MaxBytesToReadFromUSB = fh.attrs.MaxPayloadSizeFromTargetInBytes;

        if (Simulate)
        {
            InitBufferWithXMLHeader (&ReadBuffer[PacketLoc], sizeof (ReadBuffer) - PacketLoc);
            AppendToBuffer (ReadBuffer, "<data>\n", MAX_READ_BUFFER_SIZE);
            AppendToBuffer (ReadBuffer, "<response value=\"ACK\" rawmode=\"false\" ", MAX_READ_BUFFER_SIZE);
            AppendToBuffer (ReadBuffer, "/>\n</data>", MAX_READ_BUFFER_SIZE);
            CharsInBuffer = strlen (ReadBuffer);
        }


    } // end of TestDDRValidity

    GetNextPacket();  // this will set all variables, including GotACK

    if (!GotACK)
    {
        dbg (LOG_ERROR, "Please see log");
        ExitAndShowLog (1);
    }

    dbg (LOG_INFO, "=======================================================");
    dbg (LOG_INFO, "===================== SUCCESS =========================");
    dbg (LOG_INFO, "=======================================================\n\n");

    return FIREHOSE_SUCCESS;
}

void CleanseSearchPaths (void)
{
    long j, k;
    char c;

    for (j = 0; j < num_search_paths; j++)
    {
        for (k = 0; (unsigned int) k < strlen (search_path[j]); k++)
        {
            // find a slash
            c = search_path[j][k];

            if (c == FORWARDSLASH || c == BACKSLASH)
                break;
        }

        if (c != FORWARDSLASH && c != BACKSLASH)
            c = SLASH;

        k = strlen (search_path[j]);

        // Now does it end in a slash
        if (search_path[j][k - 1] != c)
        {
            search_path[j][k]   = c;
            search_path[j][k + 1] = '\0';
        }

        if (!HasAPathCharacter (search_path[j], strlen (search_path[j]) ) )
        {
            // to be here means user entered a relative path, i.e. something like path2\path3 instead of c:\path1\path2\path3
            CopyString (temp_buffer, cwd, 0, 0, strlen (cwd), MAX_PATH_SIZE, MAX_PATH_SIZE);
            CopyString (temp_buffer, search_path[j], strlen (temp_buffer), 0, strlen (search_path[j]), MAX_PATH_SIZE, MAX_PATH_SIZE);
            dbg (LOG_INFO, "'%s' changed to", search_path[j]);
            CopyString (search_path[j], temp_buffer, 0, 0, strlen (temp_buffer), MAX_PATH_SIZE, MAX_PATH_SIZE);
            dbg (LOG_INFO, "this '%s'", search_path[j]);
        }
    }
} // end of void CleanseSearchPaths(void);

