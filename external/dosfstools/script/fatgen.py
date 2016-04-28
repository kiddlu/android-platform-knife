import struct, os, sys, getopt

# Constants
SECTOR_SIZE            = 512
FAT16_ENTRY_SIZE       = 2
FAT32_ENTRY_SIZE       = 4

# Structs
fat_common_header = struct.Struct('<3s8sHBHBHHBHHHII')
fat_16_header = struct.Struct('<HBI11s8s448s2s')
fat_32_header = struct.Struct('<IHHIHH12sBBBI11s8s420s2s')
fat_32_fsi = struct.Struct('<4s480s4sII12s2s2s')

# Info dictionaries
fat_header_info = {}
fat_32_fsi_info = {}

# Global properties
fat_container_file = None
fat_type = None

def generate_fat16_container(size_mb):
    cluster_size = 16*1024
    print "Using cluster size %d bytes" % cluster_size
    num_clusters = (size_mb*1024*1024) / cluster_size
    fat_table_size = num_clusters * FAT16_ENTRY_SIZE
    fat_table_size = fat_table_size + (SECTOR_SIZE-(fat_table_size%SECTOR_SIZE))
    max_root_dir_ent = 512
    fp = open(fat_container_file, 'wb')
    
    all_bytes = [
        '\xEB\x3C\x90',                  # jump_code_nop,
        "MSDOS5.0"+(" "*(11-len("MSDOS5.0"))),                      # oem_name
        SECTOR_SIZE,                     # bytes_per_sec
        cluster_size / SECTOR_SIZE,      # sec_per_cluster
        1,                               # reserved_sec
        2,                               # num_fat_copies
        max_root_dir_ent,                # max_root_dir_ent
        0,                               # num_sec_small_32mb
        0xF8,                            # media_desc
        fat_table_size/SECTOR_SIZE,      # sec_per_fat
        63,                              # sec_per_track
        255,                             # num_heads
        0,                               # num_hidden_sec
        (size_mb*1024*1024)/SECTOR_SIZE, # num_sec
    ]
    fp.write(fat_common_header.pack(*all_bytes))
    
    if fat_type.lower() == "fat16".lower():
        all_bytes = [
            0x80,                                # logical_drive_num
            #0,                                  # reserved
            0x29,                                # ext_sign
            12345678,                            # ser_num
            "NO NAME"+(" "*(11-len("NO NAME"))), # vol_name
            "FAT16"+(" "*(8-len("FAT16"))),      # fat_name
            '\x33\xC9\x8E\xD1\xBC\xF0\x7B\x8E\xD9\xB8\x00\x20\x8E\xC0\xFC\xBD\x00\x7C\x38\x4E\x24\x7D\x24\x8B\xC1\x99\xE8\x3C\x01\x72\x1C\x83\xEB\x3A\x66\xA1\x1C\x7C\x26\x66\x3B\x07\x26\x8A\x57\xFC\x75\x06\x80\xCA\x02\x88\x56\x02\x80\xC3\x10\x73\xEB\x33\xC9\x8A\x46\x10\x98\xF7\x66\x16\x03\x46\x1C\x13\x56\x1E\x03\x46\x0E\x13\xD1\x8B\x76\x11\x60\x89\x46\xFC\x89\x56\xFE\xB8\x20\x00\xF7\xE6\x8B\x5E\x0B\x03\xC3\x48\xF7\xF3\x01\x46\xFC\x11\x4E\xFE\x61\xBF\x00\x00\xE8\xE6\x00\x72\x39\x26\x38\x2D\x74\x17\x60\xB1\x0B\xBE\xA1\x7D\xF3\xA6\x61\x74\x32\x4E\x74\x09\x83\xC7\x20\x3B\xFB\x72\xE6\xEB\xDC\xA0\xFB\x7D\xB4\x7D\x8B\xF0\xAC\x98\x40\x74\x0C\x48\x74\x13\xB4\x0E\xBB\x07\x00\xCD\x10\xEB\xEF\xA0\xFD\x7D\xEB\xE6\xA0\xFC\x7D\xEB\xE1\xCD\x16\xCD\x19\x26\x8B\x55\x1A\x52\xB0\x01\xBB\x00\x00\xE8\x3B\x00\x72\xE8\x5B\x8A\x56\x24\xBE\x0B\x7C\x8B\xFC\xC7\x46\xF0\x3D\x7D\xC7\x46\xF4\x29\x7D\x8C\xD9\x89\x4E\xF2\x89\x4E\xF6\xC6\x06\x96\x7D\xCB\xEA\x03\x00\x00\x20\x0F\xB6\xC8\x66\x8B\x46\xF8\x66\x03\x46\x1C\x66\x8B\xD0\x66\xC1\xEA\x10\xEB\x5E\x0F\xB6\xC8\x4A\x4A\x8A\x46\x0D\x32\xE4\xF7\xE2\x03\x46\xFC\x13\x56\xFE\xEB\x4A\x52\x50\x06\x53\x6A\x01\x6A\x10\x91\x8B\x46\x18\x96\x92\x33\xD2\xF7\xF6\x91\xF7\xF6\x42\x87\xCA\xF7\x76\x1A\x8A\xF2\x8A\xE8\xC0\xCC\x02\x0A\xCC\xB8\x01\x02\x80\x7E\x02\x0E\x75\x04\xB4\x42\x8B\xF4\x8A\x56\x24\xCD\x13\x61\x61\x72\x0B\x40\x75\x01\x42\x03\x5E\x0B\x49\x75\x06\xF8\xC3\x41\xBB\x00\x00\x60\x66\x6A\x00\xEB\xB0\x42\x4F\x4F\x54\x4D\x47\x52\x20\x20\x20\x20\x0D\x0A\x52\x65\x6D\x6F\x76\x65\x20\x64\x69\x73\x6B\x73\x20\x6F\x72\x20\x6F\x74\x68\x65\x72\x20\x6D\x65\x64\x69\x61\x2E\xFF\x0D\x0A\x44\x69\x73\x6B\x20\x65\x72\x72\x6F\x72\xFF\x0D\x0A\x50\x72\x65\x73\x73\x20\x61\x6E\x79\x20\x6B\x65\x79\x20\x74\x6F\x20\x72\x65\x73\x74\x61\x72\x74\x0D\x0A\x00\x00\x00\x00\x00\x00\x00\xAC\xCB\xD8', #exec_code
            '\x55\xAA',                          # exec_marker
        ]
        fp.write(fat_16_header.pack(*all_bytes))
        all_bytes = '\xF8\xFF\xFF\xFF'+((fat_table_size-4)*'\x00')
        fp.write(all_bytes)
        fp.write(all_bytes)
        all_bytes = max_root_dir_ent * 32 * '\x00'
        fp.write(all_bytes)
        all_bytes = 512 * '\x00'
        fp.write(all_bytes)    
    fp.close()

def generate_fat32_container(size_mb):
    cluster_size = 4*1024
    print "Using cluster size %d bytes" % cluster_size
    num_clusters = (size_mb*1024*1024) / cluster_size
    fat_table_size = num_clusters * FAT32_ENTRY_SIZE
    fat_table_size = fat_table_size + (SECTOR_SIZE-(fat_table_size%SECTOR_SIZE))
    fp = open(fat_container_file, 'wb')
    
    all_bytes_common = [
        '\xEB\x58\x90',                  # jump_code_nop,
        "MSDOS5.0"+(" "*(11-len("MSDOS5.0"))),                      # oem_name
        SECTOR_SIZE,                     # bytes_per_sec
        cluster_size / SECTOR_SIZE,      # sec_per_cluster
        32,                              # reserved_sec
        2,                               # num_fat_copies
        0,                               # max_root_dir_ent
        0,                               # num_sec_small_32mb #TODO - read up on this field and modfify reading/writing header accordingly
        0xF8,                            # media_desc
        0,                               # sec_per_fat
        63,                              # sec_per_track
        255,                             # num_heads
        0,                               # num_hidden_sec
        (size_mb*1024*1024)/SECTOR_SIZE, # num_sec
    ]
    fp.write(fat_common_header.pack(*all_bytes_common))
    
    if fat_type.lower() == "fat32".lower():
        all_bytes_header = [
            fat_table_size/SECTOR_SIZE,          # sectors per fat
            0,                                   # reserved
            0,                                   # version num
            2,                                   # root cluster
            1,                                   # fs info sector num
            6,                                   # backup boot sector
            12*'\x00',                           # reserved
            0x80,                                # Drive num
            0,                                   # reserved
            0x29,                                # ext_sign
            12345678,                            # ser_num
            "NO NAME"+(" "*(11-len("NO NAME"))), # vol_name
            "FAT32"+(" "*(8-len("FAT32"))),      # fat_name            
            '\x33\xC9\x8E\xD1\xBC\xF4\x7B\x8E\xC1\x8E\xD9\xBD\x00\x7C\x88\x4E\x02\x8A\x56\x40\xB4\x41\xBB\xAA\x55\xCD\x13\x72\x10\x81\xFB\x55\xAA\x75\x0A\xF6\xC1\x01\x74\x05\xFE\x46\x02\xEB\x2D\x8A\x56\x40\xB4\x08\xCD\x13\x73\x05\xB9\xFF\xFF\x8A\xF1\x66\x0F\xB6\xC6\x40\x66\x0F\xB6\xD1\x80\xE2\x3F\xF7\xE2\x86\xCD\xC0\xED\x06\x41\x66\x0F\xB7\xC9\x66\xF7\xE1\x66\x89\x46\xF8\x83\x7E\x16\x00\x75\x38\x83\x7E\x2A\x00\x77\x32\x66\x8B\x46\x1C\x66\x83\xC0\x0C\xBB\x00\x80\xB9\x01\x00\xE8\x2B\x00\xE9\x2C\x03\xA0\xFA\x7D\xB4\x7D\x8B\xF0\xAC\x84\xC0\x74\x17\x3C\xFF\x74\x09\xB4\x0E\xBB\x07\x00\xCD\x10\xEB\xEE\xA0\xFB\x7D\xEB\xE5\xA0\xF9\x7D\xEB\xE0\x98\xCD\x16\xCD\x19\x66\x60\x80\x7E\x02\x00\x0F\x84\x20\x00\x66\x6A\x00\x66\x50\x06\x53\x66\x68\x10\x00\x01\x00\xB4\x42\x8A\x56\x40\x8B\xF4\xCD\x13\x66\x58\x66\x58\x66\x58\x66\x58\xEB\x33\x66\x3B\x46\xF8\x72\x03\xF9\xEB\x2A\x66\x33\xD2\x66\x0F\xB7\x4E\x18\x66\xF7\xF1\xFE\xC2\x8A\xCA\x66\x8B\xD0\x66\xC1\xEA\x10\xF7\x76\x1A\x86\xD6\x8A\x56\x40\x8A\xE8\xC0\xE4\x06\x0A\xCC\xB8\x01\x02\xCD\x13\x66\x61\x0F\x82\x75\xFF\x81\xC3\x00\x02\x66\x40\x49\x75\x94\xC3\x42\x4F\x4F\x54\x4D\x47\x52\x20\x20\x20\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x0D\x0A\x52\x65\x6D\x6F\x76\x65\x20\x64\x69\x73\x6B\x73\x20\x6F\x72\x20\x6F\x74\x68\x65\x72\x20\x6D\x65\x64\x69\x61\x2E\xFF\x0D\x0A\x44\x69\x73\x6B\x20\x65\x72\x72\x6F\x72\xFF\x0D\x0A\x50\x72\x65\x73\x73\x20\x61\x6E\x79\x20\x6B\x65\x79\x20\x74\x6F\x20\x72\x65\x73\x74\x61\x72\x74\x0D\x0A\x00\x00\x00\x00\x00\xAC\xCB\xD8\x00\x00', #exec_code
            '\x55\xAA',                          # exec_marker
        ]
        fp.write(fat_32_header.pack(*all_bytes_header))
        
        all_bytes_fsi = [
           '\x52\x52\x61\x41',
            480*'\x00',
            '\x72\x72\x41\x61h',
            0xFFFFFFFF,
            2,
            '\x00'*12,
            '\x00'*2,
            '\x55\xAA',
        ]
        fp.write(fat_32_fsi.pack(*all_bytes_fsi))
        
        all_bytes_empty=512*'\x00'
        all_bytes_empty_sign=(510*'\x00')+'\x55\xAA'
        fp.write(all_bytes_empty_sign)
        fp.write(all_bytes_empty * (6-(2+1)))
        
        fp.write(fat_common_header.pack(*all_bytes_common))
        fp.write(fat_32_header.pack(*all_bytes_header))
        fp.write(fat_32_fsi.pack(*all_bytes_fsi))
        fp.write(all_bytes_empty_sign)
        
        fp.write(all_bytes_empty*(32-(6+1+1)-1))
        
        # write fat table
        all_bytes = '\xF8\xFF\xFF\x0F\xFF\xFF\xFF\x0F\xFF\xFF\xFF\x0F'+((fat_table_size-12)*'\x00')
        fp.write(all_bytes)
        fp.write(all_bytes)
        
    fp.close()

def usage():
    print """
Usage: python fatgen.py [OPTION...]
Generates a FAT16 or FAT32 container.

Examples:
  python fatgen.py --fat16 --name=test.bin --size=256 # Generate a 256MB FAT16 container.
  python fatgen.py --fat32 --name=test.bin --size=512 # Generate a 512MB FAT32 container.

 Options:

  -fat16, --f                FAT16
  -fat32, --t                FAT32
  -s, --size                 Size in MB
  -c, --sectors              Size in sectors
  -n, --name                 Container name
  -?, --help                 Display this help message
  -v, --verbose              Verbose messages

    """

def main():
    global fat_container_file, fat_type
    fat_size = None
    
    try:
        opts, args = getopt.getopt(sys.argv[1:], "fts:c:n:?v", ["fat16", "fat32", "size=", "sectors=", "name=", "help", "verbose"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str(err) # will print something like "option -a not recognized"
        usage()
        sys.exit(1)
    
    for o, a in opts:
        if o in ("-f", "--fat16"):
            fat_type = "fat16"
        elif o in ("-t", "--fat32"):
            fat_type = "fat32"
        elif o in ("-s", "--size"):
            fat_size = int(a)
        elif o in ("-c", "--sectors"):
            fat_size = int(((long(a)*512)/1024)/1024)
            print "Converted from sectors, container size = %dMB" % fat_size
        elif o in ("-n", "--name"):
            fat_container_file = a
        elif o in ("-?", "--help"):
            usage()
            sys.exit(0)
        elif o in ("-v", "--verbose"):
            pass
        else:
            assert False, "unhandled option"
    
    if fat_type is None:
        usage()
        print "\nERROR: No FAT type selected. Choose --fat16 or --fat32"
        sys.exit(1)
    if fat_size is None or fat_size <= 0:
        usage()
        print "\nERROR: Invalid FAT size"
        sys.exit(1)
    if (fat_type.lower()=="fat16".lower() and fat_size < 64) or (fat_type.lower()=="fat32".lower() and fat_size < 256):
        print
        print "Based on the default cluster sizes used, the container must be larger than a minimum size"
        print "in order to be correctly recognized by Windows systems. See below for minimum sizes:"
        print "FAT16 -  Default cluster size: 16384, Minimum size:  64MB"
        print "FAT32 -  Default cluster size: 4096,  Minimum size: 256MB"
        print
        sys.exit(1)
    if fat_container_file is None:
        fat_container_file = "%s_%dMB.bin" % (fat_type.upper(), fat_size)
        print "No FAT container file name provided, name will be set to %s" % fat_container_file
    
    if not os.path.exists(fat_container_file):
        if fat_type.lower()=="fat16".lower():
            generate_fat16_container(fat_size)
            print "Generated %dMB %s container: %s" % (fat_size, fat_type.upper(), fat_container_file)
        if fat_type.lower()=="fat32".lower():
            generate_fat32_container(fat_size)
            print "Generated %dMB %s container: %s" % (fat_size, fat_type.upper(), fat_container_file)
    else:
        print "File %s already exists. Exiting." % fat_container_file

if __name__ == "__main__":
    main()
