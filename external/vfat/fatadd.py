import struct, os, sys
import getopt
import time

# Constants
SECTOR_SIZE            = 512
FILE_ENTRY_SIZE        = 32
FAT_FILE_NAME_LENGTH   = 11
ATTRIBUTE_READONLY     = 0x01
ATTRIBUTE_HIDDEN       = 0x02
ATTRIBUTE_SYSTEM       = 0x04
ATTRIBUTE_VOL_LABEL    = 0x08
ATTRIBUTE_DIR          = 0x10
ATTRIBUTE_ARCHIVE      = 0x20
FAT16_ENTRY_SIZE       = 2
FAT32_ENTRY_SIZE       = 4
DIR_ENT_LENGTH         = 32

# Structs
fat_common_header = struct.Struct('<3s8sHBHBHHBHHHII')
fat_16_header = struct.Struct('<HBI11s8s448s2s')
fat_dirent = struct.Struct('<8s3sBBBHHHHHHHI')
fat_32_header = struct.Struct('<IHHIHH12sBBBI11s8s420s2s')
fat_32_fsi = struct.Struct('<4s480s4sII12s2s2s')

# Info dictionaries
fat_header_info = {}
fat_32_fsi_info = {}

# Global properties
fat_container_file = None
fat_type = None
current_cluster = None

def round_up(dividend, divisor):
    result = dividend / divisor
    if dividend % divisor != 0:
        result = result + 1
    return result

def PrintBigError(sz):
    print "\t _________________ ___________ "
    print "\t|  ___| ___ \\ ___ \\  _  | ___ \\"
    print "\t| |__ | |_/ / |_/ / | | | |_/ /"
    print "\t|  __||    /|    /| | | |    / "
    print "\t| |___| |\\ \\| |\\ \\\\ \\_/ / |\\ \\ "
    print "\t\\____/\\_| \\_\\_| \\_|\\___/\\_| \\_|\n"

    if len(sz)>0:
        print sz
        sys.exit(1)

def read_header():
    global fat_header_info, fat_type, fat_32_fsi_info
    
    try:
        fp = open(fat_container_file, 'rb')
    except:
        PrintBigError("Can't open '%s' in binary read mode" % fat_container_file)

    try:
        fp.seek(0)
    except:
        PrintBigError("Can't seek to 0 in '%s'" % fat_container_file)

    try:
        data = fp.read(fat_common_header.size)
    except:
        PrintBigError("Can't read %d bytes from '%s'" % (fat_common_header.size,fat_container_file))

    try:
        fat_data = fp.read(SECTOR_SIZE-fat_common_header.size)
    except:
        PrintBigError("Can't read %d bytes from '%s'" % (SECTOR_SIZE-fat_common_header.size,fat_container_file))

    try:
        fp.close()
    except:
        PrintBigError("Can't close '%s'" % (fat_container_file))
    
    all_bytes = fat_common_header.unpack(data)
    fat_header_info['jump_code_nop'] = all_bytes[0]
    fat_header_info['oem_name'] = all_bytes[1]
    fat_header_info['bytes_per_sec'] = all_bytes[2]
    if SECTOR_SIZE != fat_header_info['bytes_per_sec']:
        PrintBigError("Sector size mismatch. Header %d Specified %d" % (fat_header_info['bytes_per_sec'], SECTOR_SIZE))
    fat_header_info['sec_per_cluster'] = all_bytes[3]
    fat_header_info['reserved_sec'] = all_bytes[4]
    fat_header_info['num_fat_copies'] = all_bytes[5]
    fat_header_info['max_root_dir_ent'] = all_bytes[6]
    fat_header_info['num_sec_small_32mb'] = all_bytes[7]
    fat_header_info['media_desc'] = all_bytes[8]
    fat_header_info['sec_per_fat'] = all_bytes[9]
    fat_header_info['sec_per_track'] = all_bytes[10]
    fat_header_info['num_heads'] = all_bytes[11]
    fat_header_info['num_hidden_sec'] = all_bytes[12]
    fat_header_info['num_sec'] = all_bytes[13]
    print "Container size is %d bytes" % (int(fat_header_info['num_sec'])*SECTOR_SIZE)
    
    if fat_data[18:18+8].strip().lower() == 'FAT16'.lower():
        fat_type = 'fat16'
        fat_data = fat_data[:fat_16_header.size]
        all_bytes = fat_16_header.unpack(fat_data)
        fat_header_info['logical_drive_num'] = all_bytes[0]
        fat_header_info['ext_sign'] = all_bytes[1]
        fat_header_info['ser_num'] = all_bytes[2]
        fat_header_info['vol_name'] = all_bytes[3]
        fat_header_info['fat_name'] = all_bytes[4]
        #fat_header_info['exec_code'] = all_bytes[5]
        fat_header_info['exec_marker'] = all_bytes[6]
    else:
        fat_type = 'fat32'
        fat_data = fat_data[:fat_32_header.size]
        all_bytes = fat_32_header.unpack(fat_data)
        fat_header_info['old_sec_per_fat'] = fat_header_info['sec_per_fat']
        fat_header_info['sec_per_fat'] = all_bytes[0]
        fat_header_info['flags'] = all_bytes[1]
        fat_header_info['ver_num'] = all_bytes[2]
        fat_header_info['root_dir_start_cluster'] = all_bytes[3]
        fat_header_info['fsi_start_num'] = all_bytes[4]
        fat_header_info['bkp_boot_sec_num'] = all_bytes[5]
        #fat_header_info['reserved'] = all_bytes[6]
        fat_header_info['logical_drive_num'] = all_bytes[7]
        #fat_header_info['unused'] = all_bytes[8]
        fat_header_info['ext_sign'] = all_bytes[9]
        fat_header_info['ser_num'] = all_bytes[10]
        fat_header_info['vol_name'] = all_bytes[11]
        fat_header_info['fat_name'] = all_bytes[12]
        #fat_header_info['exec_code'] = all_bytes[13]
        fat_header_info['exec_marker'] = all_bytes[14]
        
        try:
            fp = open(fat_container_file, 'rb')
        except:
            PrintBigError("Can't open '%s' in binary read mode" % (fat_container_file))

        try:
            fp.seek(fat_header_info['fsi_start_num'] * SECTOR_SIZE)
        except:
            PrintBigError("Can't seek to location %d in '%s'" % (fat_header_info['fsi_start_num'] * SECTOR_SIZE,fat_container_file))

        all_bytes = fat_32_fsi.unpack(fp.read(SECTOR_SIZE))

        try:
            fp.close()
        except:
            PrintBigError("Can't close '%s'" % (fat_container_file))

        fat_32_fsi_info['fsi_sign_1'] = all_bytes[0]
        #fat_32_fsi_info['reserved'] = all_bytes[1]
        fat_32_fsi_info['fsi_sign_2'] = all_bytes[2]
        fat_32_fsi_info['num_free_clusters'] = all_bytes[3]
        fat_32_fsi_info['recent_alloc_cluster'] = all_bytes[4]
        #fat_32_fsi_info['reserved'] = all_bytes[5]
        #fat_32_fsi_info['reserved'] = all_bytes[6]
        fat_32_fsi_info['fsi_marker'] = all_bytes[7]

def clean_short_dos_name(file_name):
    if "." in file_name:
        file_part = file_name[:min(8, file_name.index("."))]
        ext_part = file_name[file_name.rindex(".")+1:file_name.rindex(".")+1+3]
        return "%s.%s" % (file_part.upper(), ext_part.upper())
    else:
        return file_name[:8].upper()

def get_cluster_sector(cluster_num):
    if fat_type.lower()=="fat16".lower():
        return fat_header_info['reserved_sec'] + (fat_header_info['num_fat_copies'] * fat_header_info['sec_per_fat']) + (round_up((fat_header_info['max_root_dir_ent']*32), fat_header_info['bytes_per_sec'])) + ((cluster_num - 2)*fat_header_info['sec_per_cluster'])
    if fat_type.lower()=="fat32".lower():
        return fat_header_info['reserved_sec'] + (fat_header_info['num_fat_copies'] * fat_header_info['sec_per_fat']) + ((cluster_num - 2)*fat_header_info['sec_per_cluster'])

def read_cluster(cluster_num):
    if get_cluster_sector(cluster_num)*fat_header_info['bytes_per_sec'] >= os.path.getsize(fat_container_file):
        write_cluster(cluster_num, '')
    fat_container_file_fp.seek(get_cluster_sector(cluster_num)*fat_header_info['bytes_per_sec'])
    return fat_container_file_fp.read(fat_header_info['sec_per_cluster']*fat_header_info['bytes_per_sec'])

def write_cluster(cluster_num, data):
    padding_data_len = (get_cluster_sector(cluster_num)*fat_header_info['bytes_per_sec']) - os.path.getsize(fat_container_file)
    if padding_data_len >= 0:
        fat_container_file_fp.seek(0, os.SEEK_END)
        fat_container_file_fp.write('\x00'*padding_data_len)
        fat_container_file_fp.seek(0, os.SEEK_END)
    else:
        fat_container_file_fp.seek(get_cluster_sector(cluster_num)*fat_header_info['bytes_per_sec'])
    data = data + '\x00'*((fat_header_info['sec_per_cluster']*fat_header_info['bytes_per_sec'])-len(data))
    fat_container_file_fp.write(data)
    
def get_cluster_table():
    fat_container_file_fp.seek(fat_header_info['reserved_sec']*fat_header_info['bytes_per_sec'])
    cluster_table = fat_container_file_fp.read(fat_header_info['sec_per_fat']*fat_header_info['bytes_per_sec'])
    if fat_type.lower() == "fat16".lower():
        num_entries = round_up(len(cluster_table), FAT16_ENTRY_SIZE)
        temp = struct.unpack("<%dh" % num_entries, cluster_table)
        cluster_table = [x & 0xffff for x in temp]
    if fat_type.lower() == "fat32".lower():
        num_entries = round_up(len(cluster_table), FAT32_ENTRY_SIZE)
        temp = struct.unpack("<%di" % num_entries, cluster_table)
        cluster_table = [x & 0xffffffff for x in temp]
    return cluster_table

def find_free_cluster():
    cluster_table = get_cluster_table()
    if 0 in cluster_table:
        return cluster_table.index(0)
        #return cluster_table.index(0) == 2 and 3 or cluster_table.index(0)
    return None

def update_fat_cluster(cluster_num, next_cluster=None):
    offset = None
    value = None
    if fat_type.lower()=="fat16".lower():
        offset = cluster_num * FAT16_ENTRY_SIZE
        if next_cluster is None:
            next_cluster = 0xffff
        value = struct.pack("<H", 0xffff & next_cluster)
    if fat_type.lower()=="fat32".lower():
        offset = cluster_num * FAT32_ENTRY_SIZE
        if next_cluster is None:
            next_cluster = 0xffffffff
        value = struct.pack("<I", 0xffffffff & next_cluster)
    fat_container_file_fp.seek((fat_header_info['reserved_sec']*fat_header_info['bytes_per_sec'])+offset)
    fat_container_file_fp.write(value)
    fat_container_file_fp.seek(((fat_header_info['reserved_sec']+fat_header_info['sec_per_fat'])*fat_header_info['bytes_per_sec'])+offset)
    fat_container_file_fp.write(value)
    """
    fat_container_file_fp.seek((fat_header_info['reserved_sec']*fat_header_info['bytes_per_sec'])+next_cluster)
    if fat_type.lower()=="fat16".lower():
        fat_container_file_fp.write(struct.pack("<h", 0xffff))
    if fat_type.lower()=="fat32".lower():
        fat_container_file_fp.write(struct.pack("<i", 0xffffffff))
    """

def get_cluster_chain(start_cluster_num):
    cluster_table = get_cluster_table()
    cluster_chain = [start_cluster_num]
    if fat_type=="fat16":
        valid_lower_bound = 0x0002
        valid_upper_bound = 0xFFEF
        #end_lower_bound = 0xFFF8
        #end_upper_bound = 0xFFFF
    if fat_type=="fat32":
        valid_lower_bound = 0x00000002
        valid_upper_bound = 0x0FFFFFEF
        #end_lower_bound = 0x0FFFFFF8
        #end_upper_bound = 0x0FFFFFFF
    while cluster_table[start_cluster_num] >= valid_lower_bound and cluster_table[start_cluster_num] <= valid_upper_bound:
        cluster_chain.append(cluster_table[start_cluster_num])
        start_cluster_num = cluster_table[start_cluster_num]
    return cluster_chain

def add_file_entry(real_file_name, file_name, dir_cluster=None, dir_table_start=None, dir_table_length=None):
    free_cluster = find_free_cluster()
    update_fat_cluster(free_cluster)
    print "adding", file_name
    dot_index = file_name.rindex(".")
    file_part_name = file_name[:dot_index]
    ext_part_name = file_name[dot_index+1:]
    file_size = os.path.getsize(real_file_name)
    atime = time.localtime(os.path.getatime(real_file_name))
    ctime = time.localtime(os.path.getctime(real_file_name))
    mtime = time.localtime(os.path.getmtime(real_file_name))
    #tm_year=2011, tm_mon=7, tm_mday=6, tm_hour=12, tm_min=23, tm_sec=16, tm_wday=2, tm_yday=187, tm_isdst=1
    #((mtime[3]<<11) & 0xF800) | ((mtime[4]<<5) & 0x7E0) | ((mtime[5]/2) & 0x1F)
    #(((mtime[0]-1980)<<9) & 0xFE00) | ((mtime[1]<<5) & 0x1E0) | (mtime[2] & 0x1F)
    
    if fat_type.lower()=="fat16".lower():
        dirent = fat_dirent.pack(file_part_name+('\x20'*(8-len(file_part_name))), ext_part_name+('\x20'*(3-len(ext_part_name))), 0x20, 0x18, 0x0, ((ctime[3]<<11) & 0xF800) | ((ctime[4]<<5) & 0x7E0) | ((ctime[5]/2) & 0x1f), (((ctime[0]-1980)<<9) & 0xFE00) | ((ctime[1]<<5) & 0x1E0) | (ctime[2] & 0x1F), (((atime[0]-1980)<<9) & 0xFE00) | ((atime[1]<<5) & 0x1E0) | (atime[2] & 0x1F), 0x0000, ((mtime[3]<<11) & 0xF800) | ((mtime[4]<<5) & 0x7E0) | ((mtime[5]/2) & 0x1F), (((mtime[0]-1980)<<9) & 0xFE00) | ((mtime[1]<<5) & 0x1E0) | (mtime[2] & 0x1F), free_cluster, file_size)
    if fat_type.lower()=="fat32".lower():
        dirent = fat_dirent.pack(file_part_name+('\x20'*(8-len(file_part_name))), ext_part_name+('\x20'*(3-len(ext_part_name))), 0x20, 0x18, 0x0, 0x70EA, 0x3EE7, 0x3EE7, free_cluster & 0xff00, 0x8884, 0x3EE5, free_cluster & 0xff, file_size)
    block_size = fat_header_info['sec_per_cluster']*fat_header_info['bytes_per_sec']
    offset = None
    if fat_type=="fat16" and dir_cluster is None:
        fat_container_file_fp.seek(dir_table_start)
        dir_cluster_data = fat_container_file_fp.read(dir_table_length)
        for i in range(dir_table_start, dir_table_start+dir_table_length, DIR_ENT_LENGTH):
            dir_data = fat_dirent.unpack(dir_cluster_data[i-dir_table_start:i-dir_table_start+DIR_ENT_LENGTH])
            if dir_data[0].strip('\x00').strip()=="":
                offset = i
                break
    else:
        dir_cluster_data=read_cluster(dir_cluster)
        for i in range(0, len(dir_cluster_data), DIR_ENT_LENGTH):
            dir_data = fat_dirent.unpack(dir_cluster_data[i:i+DIR_ENT_LENGTH])
            if dir_data[0].strip('\x00').strip()=="":
                offset = i + (get_cluster_sector(dir_cluster)*fat_header_info['bytes_per_sec'])
                break
    
    if not offset is None:
        fat_container_file_fp.seek(offset)
        fat_container_file_fp.write(dirent)

        try:
            fp = open(real_file_name, 'rb')
        except:
            PrintBigError("Can't open '%s' in binary read mode" % (real_file_name))
        
        while file_size > 0:
            write_cluster(free_cluster, fp.read(min(block_size, file_size)))
            file_size = file_size - min(block_size, file_size)
            if file_size > 0:
                new_free_cluster = find_free_cluster()
                update_fat_cluster(free_cluster, new_free_cluster)
                free_cluster = new_free_cluster
            update_fat_cluster(free_cluster)

        try:
            fp.close()
        except:
            PrintBigError("Can't close '%s'" % (real_file_name))
    
def add_directory_entry(dir_name, dir_cluster=None, dir_table_start=None, dir_table_length=None):
    free_cluster = find_free_cluster()
    print "adding", dir_name
    dirent = fat_dirent.pack(dir_name+('\x20'*(8-len(dir_name))), 3*'\x20', 0x10, 0x08, 0x80, 0x70EA, 0x3EE7, 0x3EE7, 0x0000, 0x8BC8, 0x3EE5, free_cluster, 0)
    offset = None
    if fat_type=="fat16" and dir_cluster is None:
        fat_container_file_fp.seek(dir_table_start)
        dir_cluster_data = fat_container_file_fp.read(dir_table_length)
        for i in range(dir_table_start, dir_table_start+dir_table_length, DIR_ENT_LENGTH):
            dir_data = fat_dirent.unpack(dir_cluster_data[i-dir_table_start:i-dir_table_start+DIR_ENT_LENGTH])
            if dir_data[0].strip('\x00').strip()=="":
                offset = i
                break
    else:
        dir_cluster_data=read_cluster(dir_cluster)
        for i in range(0, len(dir_cluster_data), DIR_ENT_LENGTH):
            dir_data = fat_dirent.unpack(dir_cluster_data[i:i+DIR_ENT_LENGTH])
            if dir_data[0].strip('\x00').strip()=="":
                offset = i + (get_cluster_sector(dir_cluster)*fat_header_info['bytes_per_sec'])
                break
    
    if not offset is None:
        fat_container_file_fp.seek(offset)
        fat_container_file_fp.write(dirent)
        if dir_cluster is None:
            temp = 0
        else:
            temp = dir_cluster
        write_cluster(free_cluster, fat_dirent.pack('.'+('\x20'*(8-len('.'))), 3*'\x20', 0x10, 0, 0x93, 0x70EA, 0x3EE7, 0x3EE7, 0x0000, 0x70EB, 0x3EE7, free_cluster, 0) + fat_dirent.pack('..'+('\x20'*(8-len('..'))), 3*'\x20', 0x10, 0, 0x93, 0x70EA, 0x3EE7, 0x3EE7, 0x0000, 0x70EB, 0x3EE7, temp, 0))
        update_fat_cluster(free_cluster)
    return free_cluster

def find_directory_entry(dir_name, dir_cluster=None, dir_table_start=None, dir_table_length=None):
    global current_cluster
    
    if fat_type=="fat16" and dir_cluster is None:
        fat_container_file_fp.seek(dir_table_start)
        dir_cluster_data = fat_container_file_fp.read(dir_table_length)
        return find_directory_entry_helper(dir_name, dir_cluster_data)
    else:
        cluster_chain = get_cluster_chain(dir_cluster)
        for each_cluster in cluster_chain:
            result = find_directory_entry_helper(dir_name, read_cluster(each_cluster))
            if not result is None:
                if result is False:
                    current_cluster = each_cluster
                return result
        current_cluster = each_cluster
    return None

def find_directory_entry_helper(dir_name, dir_cluster_data):
    """
    True: directory exists, current_cluster holds the cluster number of the directory
    False: directory does not exist, but there's space in the current searched cluster for it
    None: directory does not exist, no space found for it in the current searched cluster
    """
    global current_cluster
    for i in range(0, len(dir_cluster_data), DIR_ENT_LENGTH):
        dir_data = fat_dirent.unpack(dir_cluster_data[i:i+DIR_ENT_LENGTH])
        if dir_data[0].strip('\x00').strip().upper()==dir_name:
            current_cluster = dir_data[11]
            return True
        if dir_data[0].strip('\x00').strip()=="":
            return False
    return None

def add_entry(fat_file_name, real_file_name):
    global current_cluster
    
    fat_entry_list = []
    while fat_file_name != '':
        temp = os.path.split(fat_file_name)
        fat_entry_list.insert(0, temp[1])
        fat_file_name = temp[0]
    fat_entry_list = map(clean_short_dos_name, fat_entry_list)
    root_entry=True
    if fat_type.lower()=="fat16".lower():
        # zero-indexed start address
        dir_table_start = ( fat_header_info['reserved_sec'] + (fat_header_info['num_fat_copies'] * fat_header_info['sec_per_fat']) ) * fat_header_info['bytes_per_sec']
        # length in bytes (of this chunk of directory info)
        dir_table_length = (fat_header_info['max_root_dir_ent'] * 32)    
    else:
        current_cluster = fat_header_info['root_dir_start_cluster']
    while fat_entry_list:
        #import pdb; pdb.set_trace()
        if 1 == len(fat_entry_list):
            # add file
            if root_entry and fat_type.lower()=="fat16".lower():
                add_file_entry(real_file_name, fat_entry_list[0], None, dir_table_start, dir_table_length)
            else:
                add_file_entry(real_file_name, fat_entry_list[0], current_cluster)
        else:
            # add dir
            if root_entry and fat_type.lower()=="fat16".lower():
                dir_entry = find_directory_entry(fat_entry_list[0], None, dir_table_start, dir_table_length)
                if dir_entry is None:
                    print "Root directory table is full."
                    sys.exit(1)
                elif dir_entry==False:
                    current_cluster = add_directory_entry(fat_entry_list[0], None, dir_table_start, dir_table_length)
            else:
                dir_entry = find_directory_entry(fat_entry_list[0], current_cluster)
                if dir_entry is None:
                    free_cluster = find_free_cluster()
                    update_fat_cluster(current_cluster, free_cluster)
                    update_fat_cluster(free_cluster)
                    current_cluster = add_directory_entry(fat_entry_list[0], free_cluster)
                elif dir_entry==False:
                    current_cluster = add_directory_entry(fat_entry_list[0], current_cluster)
        
        print "Added", fat_entry_list[0]
        del fat_entry_list[0]
        if root_entry:
            root_entry = False

def usage():
    print """
Usage: python fatadd.py [OPTION...]
Add files to a FAT16 or FAT32 container.

Examples:
  python fatadd.py --name=FAT16_256MB.bin --from=C:\TEMP\                  # Add files and directories recursively from C:\TEMP into the
                                                                           # root directory of the container FAT16_256MB.bin
  python fatadd.py --name=FAT16_256MB.bin --from=C:\TEMP\ --dir=images     # Add files and directories recursively from C:\TEMP into the 
                                                                           # images directory of the container FAT16_256MB.bin
  python fatadd.py --name=FAT16_256MB.bin --from=C:\TEMP\myfile.txt        # Add C:\TEMP\myfile.txt into the
                                                                           # root directory of the container FAT16_256MB.bin
  Any non-existent directories along the way will be created.

 Options:

  -d, --dir                  Directory to begin with (root, if omitted)
  -n, --name                 Container name
  -f, --from                 File/directory to add into the container
  -c, --sectorsize           Sector size to use
  -?, --help                 Display this help message
  -v, --verbose              Verbose messages

    """

def main():
    global fat_container_file, fat_type, fat_container_file_fp, SECTOR_SIZE
    
    dir_name = None
    entry_name = None
    
    try:
        opts, args = getopt.getopt(sys.argv[1:], "d:n:f:c:?v", ["dir=", "name=", "from=", "sectorsize=", "help", "verbose"])
    except getopt.GetoptError, err:
        # print help information and exit:
        print str(err) # will print something like "option -a not recognized"
        usage()
        sys.exit(1)
    
    for o, a in opts:
        if o in ("-d", "--dir"):
            dir_name = a
        elif o in ("-n", "--name"):
            fat_container_file = a
        elif o in ("-f", "--from"):
            entry_name = a
        elif o in ("-c", "--sectorsize"):
            SECTOR_SIZE = int(a)
        elif o in ("-?", "--help"):
            usage()
            sys.exit(0)
        elif o in ("-v", "--verbose"):
            pass
        else:
            assert False, "unhandled option"
    
    if fat_container_file is None:
        usage()
        print "ERROR: No FAT container provided."
        sys.exit(1)
    if entry_name is None:
        usage()
        print "ERROR: No entries were provided to add to container."
        sys.exit(1)
    if dir_name is None:
        dir_name = ''
    
    read_header()

    try:
        fat_container_file_fp=open(fat_container_file, 'r+b')
    except:
        PrintBigError("Can't open '%s' in binary read/write mode" % fat_container_file)

    if fat_type.lower()=="fat32".lower():
        update_fat_cluster(fat_header_info['root_dir_start_cluster'])
    if os.path.isfile(entry_name):
        add_entry(os.path.join(dir_name, os.path.basename(entry_name)), entry_name)
    else:
        entry_name = os.path.normpath(entry_name)
        for root, dirs, files in os.walk(entry_name):
            for f in files:
                real_file_name = os.path.join(root, f)
                fat_file_name = real_file_name[real_file_name.index(entry_name)+len(entry_name):]
                fat_file_name = fat_file_name.strip("\\").strip("/")
                add_entry(os.path.join(dir_name, fat_file_name), real_file_name)
    fat_container_file_fp.close()

if __name__ == "__main__":
    main()
