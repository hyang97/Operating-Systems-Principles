#!/usr/bin python

# csv parser
import csv

# list of tuples
# each row is a tuple containing each row of the corresponding csvfiles
# except for super_csv
super_csv = []
group_csv = []
bitmap_csv = []
inode_csv = []
directory_csv = []
indirect_csv = []

# classes for inodes and blocks

class Inode:
    def __init__(self, num, ref_list, num_links, ptrs):
        self.num = num
        self.ref_list = ref_list # <directory inode, entry #>
        self.num_links = num_links
        self.ptrs = ptrs

class Block:
    def __init__(self, num, ref_list):
        self.num = num
        self.ref_list = ref_list # <inode #, indirect block #, entry #>

# data structures (to be populated by csv files)

# list/set
inode_bitmaps = [] # for free inode bitmap blocks
block_bitmaps = [] # for free block bitmap blocks
free_inodes = [] # for free inodes
free_blocks = [] # for free blocks

# hashmap
allocated_inodes = {} # <inode #, inode>
allocated_blocks = {} # <block #, block>
indirect_table = {} # <(block #, entry #), block ptr>
directory_map = {} # <child/entry inode #, parent dir inode #>

# global file system variables (to be read in from csv files)
block_size = 0
blocks_per_group = 0
inodes_per_group = 0
total_number_blocks = 0
total_number_inodes = 0
total_number_groups = 0

# dictionary/list of objects containing data for each error type
error_1 = {} # <block_num, block>
error_2 = {} # <block_num, block>
error_3 = {} # <inode_num, inode>
error_4 = [] # (inode_num, free_list_block_num)
error_5 = [] # (inode_num, reported_link_count, actual_link_count)
error_6 = [] # (inode_num, entry_name, reported_inode_num, actual_inode_num)
error_7 = [] # (block_num, inode_num, indirect_block_num, entry_num)


def read_csv():

    global super_csv
    global group_csv
    global bitmap_csv
    global inode_csv
    global directory_csv
    global indirect_csv
    
    with open('super.csv', 'r') as csvfile:
        for r in csv.reader(csvfile):
            row = [ int(r[0], 16), int(r[1], 10), int(r[2], 10),
                    int(r[3], 10), int(r[4], 10), int(r[5], 10),
                    int(r[6], 10), int(r[7], 10), int(r[8], 10) ]
            super_csv.append(row)

    with open('group.csv', 'r') as csvfile:
        for r in csv.reader(csvfile):
            row = [ int(r[0], 10), int(r[1], 10), int(r[2], 10),
                    int(r[3], 10), int(r[4], 16), int(r[5], 16),
                    int(r[6], 16) ]
            group_csv.append(row)

    with open('bitmap.csv', 'r') as csvfile:
        for r in csv.reader(csvfile):
            row = [ int(r[0], 16), int(r[1], 10) ]
            bitmap_csv.append(row)

    with open('inode.csv', 'r') as csvfile:
        for r in csv.reader(csvfile):
            row = [ int(r[0], 10), r[1], int(r[2], 8),
                    int(r[3], 10), int(r[4], 10), int(r[5], 10),
                    int(r[6], 16), int(r[7], 16), int(r[8], 16),
                    int(r[9], 10), int(r[10], 10), int(r[11], 16),
                    int(r[12], 16), int(r[13], 16), int(r[14], 16),
                    int(r[15], 16), int(r[16], 16), int(r[17], 16),
                    int(r[18], 16), int(r[19], 16), int(r[20], 16),
                    int(r[21], 16), int(r[22], 16), int(r[23], 16),
                    int(r[24], 16), int(r[25], 16) ]
            inode_csv.append(row)

    with open('directory.csv', 'r') as csvfile:
        for r in csv.reader(csvfile):
            row = [ int(r[0], 10), int(r[1], 10), int(r[2], 10),
                    int(r[3], 10), int(r[4], 10), r[5] ]
            directory_csv.append(row)

    with open('indirect.csv', 'r') as csvfile:
        for r in csv.reader(csvfile):
            row = [ int(r[0], 16), int(r[1], 10), int(r[2], 16) ]
            indirect_csv.append(row)

def unallocated_block(block):
    global error_1
    error_1[block.num] = block

def duplicate_block(block):
    global error_2
    error_2[block.num] = block

def unallocated_inode(c_inode_num, ref_list):
    global error_3
    if c_inode_num in error_3:
        error_3[c_inode_num] += ref_list
    else:
        error_3[c_inode_num] = ref_list
    
def missing_inode(inode_num, group_num):
    global error_4
    error_4.append((inode_num, group_csv[group_num][4]))

def incorrect_link_count(inode_num, reported_link_count, actual_link_count):
    global error_5
    error_5.append((inode_num, reported_link_count, actual_link_count))
    
def incorrect_dir_entry(inode_num, entry_name, reported_inode_num, actual_inode_num):
    global error_6
    error_6.append((inode_num, entry_name, reported_inode_num, actual_inode_num))

def invalid_block_ptr(block_num, inode_num, indirect_block_num, entry_num):
    global error_7
    error_7.append((block_num, inode_num, indirect_block_num, entry_num))

def initialize_globals():
    global block_size
    global blocks_per_group
    global inodes_per_group
    global total_number_blocks
    global total_number_inodes
    global total_number_groups
    
    sb = super_csv[0]
    block_size = sb[3]
    blocks_per_group = sb[5]
    inodes_per_group = sb[6]
    total_number_blocks = sb[2]
    total_number_inodes = sb[1]
    total_number_groups = len(group_csv)

def populate_lists():
    global inode_bitmaps, block_biotmaps, free_inodes, free_blocks
    
    for row in group_csv:
        inode_bitmaps.append(row[4])
        block_bitmaps.append(row[5])
    for row in bitmap_csv:
        if row[0] in inode_bitmaps:
            free_inodes.append(row[1])
        elif row[0] in block_bitmaps:
            free_blocks.append(row[1])

def populate_allocated_inodes():
    global allocated_inodes
    
    for i in inode_csv:
        new_inode = Inode(i[0], [], i[5], i[11:26])
        allocated_inodes[i[0]] = new_inode

def populate_indirect_table():
    global indirect_table
    
    for i in indirect_csv:
        indirect_table[ (i[0], i[1]) ] = i[2]
    
def update_block(block_num, inode_num, indirect_block_num, entry_num):
    global allocated_blocks

    if block_num <= 0 or block_num >= total_number_blocks:
        invalid_block_ptr(block_num, inode_num, indirect_block_num, entry_num)
    elif block_num in allocated_blocks:
        allocated_blocks[block_num].ref_list.append( (inode_num, indirect_block_num, entry_num) )
    else:
        new_block = Block(block_num, [ (inode_num, indirect_block_num, entry_num) ])
        allocated_blocks[block_num] = new_block

def populate_allocated_blocks():
        
    for i in inode_csv:
        num_of_blocks = i[10]
        inode_num = i[0]
        curr_inode = allocated_inodes[inode_num]

        if num_of_blocks <= 12:
            for x in range(num_of_blocks):
                update_block(curr_inode.ptrs[x], inode_num, 0, x)
        else:
            for x in range(12):
            	update_block(curr_inode.ptrs[x], inode_num, 0, x)
            # search through indirect blocks, anticipate sparseness
            blocks_added = 12 # keep track of the number of inode blocks proccessed 
            index = 12 # index within inode ptrs[], should run from 12 - 14
            entry = 0 # entry within indirect block, should run from 0 - 255
            while blocks_added < num_of_blocks and index < 15:
                indirect_ptr = curr_inode.ptrs[index]
                if indirect_ptr == 0 or entry >= 256:
                    index += 1
                else:
                    if (indirect_ptr, entry) in indirect_table:
                        update_block(indirect_table[(indirect_ptr, entry)], inode_num, indirect_ptr, entry)
                        blocks_added += 1
                        entry += 1
                    else:
                        entry += 1
                    

def populate_directory_map():
    global directory_map, allocated_inodes
    directory_map[2] = 2
    for e in directory_csv:
        p_inode_num = e[0]
        c_inode_num = e[4]
        entry_num = e[1]
        if entry_num > 1:
            directory_map[c_inode_num] = p_inode_num
        if c_inode_num in allocated_inodes:
            allocated_inodes[c_inode_num].ref_list.append((p_inode_num, entry_num))
        else:
            unallocated_inode(c_inode_num, [(p_inode_num, entry_num)])

def check_directory_map():
    for e in directory_csv:
        p_inode_num = e[0]
        c_inode_num = e[4]
        entry_num = e[1]
        if entry_num == 0 and c_inode_num != p_inode_num:
            incorrect_dir_entry(p_inode_num, e[5], c_inode_num, p_inode_num)
        elif entry_num == 1 and c_inode_num != directory_map[p_inode_num]:
            if p_inode_num == 2:
                if c_inode_num != 2:
                    incorrect_dir_entry(p_inode_num, e[5], c_inode_num, 2)
            else:
                incorrect_dir_entry(p_inode_num, e[5], c_inode_num, directory_map[p_inode_num]) 

def check_allocated_inodes():
    for key in allocated_inodes:
        inode = allocated_inodes[key]
        if len(inode.ref_list) == 0 and inode.num not in free_inodes and inode.num > 11:
            missing_inode(inode.num, inode.num // inodes_per_group)
        elif len(inode.ref_list) != inode.num_links:
            incorrect_link_count(inode.num, inode.num_links,
    len(inode.ref_list))
        if inode.num in free_inodes:
            unallocated_inode(inode.num, inode.ref_list)

def check_for_missing_inodes():
    for x in range(12, total_number_inodes + 1):
        if x not in free_inodes and x not in allocated_inodes:
            missing_inode(x, x // inodes_per_group)

def check_allocated_blocks():
    for key in allocated_blocks:
        block = allocated_blocks[key]
        if block.num in free_blocks:
            unallocated_block(block)
        if len(block.ref_list) > 1:
            duplicate_block(block)
            
# write all 7 error types to output file from their respective data structs 
def write_errors():
    with open('lab3b_check.txt', 'w') as output:
        
        for key in error_1:
            output.write('UNALLOCATED BLOCK < {0} > REFERENCED BY'.format(key))
            for i_num, indir_num, e_num in error_1[key].ref_list:
                if indir_num == 0:
                    output.write(' INODE < {0} > ENTRY < {1} >'.format(i_num, e_num))   
                elif indir_num != 0:
                    output.write(' INODE < {0} > INDIRECT BLOCK < {1} > ENTRY < {2} >'.format(i_num, indir_num, e_num))
            output.write('\n')

        for key in error_2:
            output.write('MULTIPLY REFERENCED BLOCK < {0} > BY'.format(key))
            for i_num, indir_num, e_num in error_2[key].ref_list:
                if indir_num == 0:
                    output.write(' INODE < {0} > ENTRY < {1} >'.format(i_num, e_num))
                elif indir_num != 0:
                    output.write(' INODE < {0} > INDIRECT BLOCK < {1} > ENTRY < {2} >'.format(i_num, indir_num, e_num))
            output.write('\n')

        for key in error_3:
            output.write('UNALLOCATED INODE < {0} > REFERENCED BY'.format(key))
            for d_inode_num, e_num in error_3[key]:
                output.write(' DIRECTORY < {0} > ENTRY < {1} >'.format(d_inode_num, e_num))
            output.write('\n')

        for i_num, f_block_num in error_4:
            output.write('MISSING INODE < {0} > SHOULD BE IN FREE LIST < {1} >\n'.format(i_num, f_block_num))

        for i_num, r_link_cnt, a_link_cnt in error_5:
            output.write('LINKCOUNT < {0} > IS < {1} > SHOULD BE < {2} >\n'.format(i_num, r_link_cnt, a_link_cnt))

        for i_num, e_name, r_i_num, a_i_num in error_6:
            output.write('INCORRECT ENTRY IN < {0} > NAME < {1} > LINK TO < {2} > SHOULD BE < {3} >\n'.format(i_num, e_name, r_i_num, a_i_num))

        for b_num, i_num, indir_num, e_num in error_7:
            if indir_num == 0:
                output.write('INVALID BLOCK < {0} > IN INODE < {1} > ENTRY < {2} >\n'.format(b_num, i_num, e_num))
            elif indir_num != 0:
                output.write('INVALID BLOCK < {0} > IN INODE < {1} > INDIRECT BLOCK < {2} > ENTRY < {3} > \n'.format(b_num, i_num, indir_num, e_num)) 

if __name__ == '__main__':

    read_csv()

    initialize_globals()
    
    populate_lists()
    populate_allocated_inodes()
    populate_indirect_table()
    populate_allocated_blocks()
    populate_directory_map()

    print "done populating data structures"
    
    check_directory_map()
    print "checking directory map"
    check_allocated_inodes()
    print "checking allocated inodes"
    check_for_missing_inodes()
    print "checking for missing inodes"
    check_allocated_blocks()
    print "checking allocated blocks"

    write_errors()
