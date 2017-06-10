#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_GROUP_DES_SIZE 32
#define EXT2_INODE_ENTRY_SIZE 128
#define EXT2_FILETYPE_FILE 0x8
#define EXT2_FILETYPE_DIRECTORY 0x4
#define EXT2_FILETYPE_SYMLINK 0xA

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

/* ------------------------- */
/* Global file system values */
/* ------------------------- */


// set by get_superblock 
unsigned long long file_system_size = 0;
unsigned int total_blocks = 0;
unsigned int total_inodes = 0;
unsigned int block_size = 0;
unsigned int blocks_per_group = 0;
unsigned int inodes_per_group = 0;

// set by get_group_descriptors
unsigned int total_block_groups = 0;

// set by get_free_bitmap_entries
unsigned int total_free_entries = 0;
unsigned int total_allocated_inodes = 0;

// set by get_directory_entries
unsigned int num_dir_entries = 0;

// set by get_indirect_block_entries
unsigned int id_entry_count = 0;

/* ----------------------------------------- */
/* Structs for important file system objects */
/* ----------------------------------------- */

typedef struct superblock{
  unsigned int magic_number;         //hex
  unsigned int total_inodes;         //dec
  unsigned int total_blocks;         //dec
  unsigned int block_size;           //dec
  int fragment_size;                 //dec (this can somehow be negative?)
  unsigned int blocks_per_group;     //dec
  unsigned int inodes_per_group;     //dec
  unsigned int fragments_per_group;  //dec
  unsigned int first_data_block;     //dec
} superblock;

typedef struct group_descriptor{
  unsigned int contained_blocks;    //dec
  unsigned int free_blocks;         //dec
  unsigned int free_inodes;         //dec
  unsigned int num_directories;     //dec
  unsigned int inode_bitmap_block;  //hex
  unsigned int block_bitmap_block;  //hex
  unsigned int inode_table_block;   //hex
} group_descriptor;

typedef struct free_bitmap_entry{
  unsigned int block_num;  //hex
  unsigned int entry_num;  //dec
} free_bitmap_entry;

typedef struct inode{
  unsigned int inode_num;      //dec
  char file_type;              //char
  unsigned int mode;           //oct
  unsigned int owner;          //dec
  unsigned int group;          //dec
  unsigned int link_count;     //dec
  unsigned int create_time;    //hex
  unsigned int mod_time;       //hex
  unsigned int access_time;    //hex
  unsigned int file_size;      //dec
  unsigned int num_blocks;     //dec
  unsigned int block_ptrs[15]; //hex
} inode;

typedef struct directory_entry{
  unsigned int parent_inode;          //dec
  unsigned int entry_num;             //dec
  unsigned int entry_length;          //dec
  unsigned int name_length;           //dec
  unsigned int file_entry_inode_num;  //dec
  char *name;                         //string
} directory_entry;

typedef struct indirect_block_entry{
  unsigned int block_num;      //hex
  unsigned int entry_num;      //dec
  unsigned int block_ptr_val;  //hex
} indirect_block_entry;


/* --------------------- */
/* Function declarations */
/* --------------------- */

int Open(const char *pathname, int flags, char *debug);
int Creat(const char *pathname, mode_t mode, char *debug);
ssize_t Pread(int fd, void *buf, size_t count, off_t offset, char *debug);
void *Malloc(size_t size, char *debug);
FILE *Fdopen(int fd, const char *mode, char *debug);
void *Realloc(void *ptr, size_t new_size, char *debug);

unsigned int extract_u_int(unsigned char *buf, int length);
int extract_s_int(char *buf, int length);
unsigned int **get_datablock_ptrs(int fs_fd, inode* fs_inodes);

void write_superblock(superblock sb);
void write_group_descriptor(group_descriptor *all_gds);
void write_free_bitmap_entry(free_bitmap_entry *all_fbes);
void write_inode(inode *all_in);
void write_directory_entry(directory_entry *all_des);
void write_indirect_block_entry(indirect_block_entry *all_ides);

superblock get_superblock(int fs_fd);
group_descriptor *get_group_descriptors(int fs_fd);
free_bitmap_entry *get_free_bitmap_entries(int fs_fd, group_descriptor *fs_all_gds);
inode *get_inodes(int fs_fd, group_descriptor *fs_all_gds);
directory_entry *get_directory_entries(int fs_fd, inode* fs_inodes);
indirect_block_entry *get_indirect_block_entries(int fs_fd, inode* fs_inodes);


/* ------------- */
/* MAIN FUNCTION */
/* ------------- */
int main(int argc, char **argv){
  char *disk_path;
  if(argc != 2){
    fprintf(stderr, "Wrong argument count, pass in just one file system image!");
    exit(1);
  }

  disk_path = argv[1];
  int disk_fd = Open(disk_path, O_RDONLY, "file system image");

  superblock disk_sb = get_superblock(disk_fd);
  group_descriptor *disk_all_gds = get_group_descriptors(disk_fd);
  free_bitmap_entry *disk_all_fbes = get_free_bitmap_entries(disk_fd, disk_all_gds);
  inode *disk_all_inodes = get_inodes(disk_fd, disk_all_gds);
  directory_entry *disk_all_dir_entries = get_directory_entries(disk_fd, disk_all_inodes);
  indirect_block_entry *disk_all_id_entries = get_indirect_block_entries(disk_fd, disk_all_inodes);
  
  write_superblock(disk_sb);
  write_group_descriptor(disk_all_gds);
  write_free_bitmap_entry(disk_all_fbes);
  write_inode(disk_all_inodes);
  write_directory_entry(disk_all_dir_entries);
  write_indirect_block_entry(disk_all_id_entries);

  close(disk_fd);
  free(disk_all_gds);
  free(disk_all_fbes);
  return 0;
}


/* ------------------------------------ */
/* Self defined functions for debugging */
/* ------------------------------------ */
int Open(const char *pathname, int flags, char *debug){
  int fd = open(pathname, flags);
  if(fd < 0){
    fprintf(stderr, "Error opening: %s\n", debug);
    exit(1);
  }
  return fd;
}

int Creat(const char *pathname, mode_t mode, char *debug){
  int fd = creat(pathname, mode);
  if(fd < 0){
    fprintf(stderr, "Error creating: %s\n", debug);
    exit(1);
  }
  return fd;
}

ssize_t Pread(int fd, void *buf, size_t count, off_t offset, char *debug){
  int bytes_read = pread(fd, buf, count, offset);
  if(bytes_read < 0){
    fprintf(stderr, "Error using pread: %s\n", debug);
    exit(1);
  }
  return bytes_read;
}

void *Malloc(size_t size, char *debug){
  void *ret_ptr = malloc(size);
  if(ret_ptr == NULL){
    fprintf(stderr, "Error allocating memory: %s\n", debug);
    exit(1);
  }
  return ret_ptr;
}

FILE *Fdopen(int fd, const char *mode, char *debug){
  FILE *fp = fdopen(fd, mode);
  if(fp == NULL){
    fprintf(stderr, "Error opening file stream: %s\n", debug);
    exit(1);
  }
  return fp;
}

void *Realloc(void *ptr, size_t new_size, char *debug){
  
  void *ret_ptr = realloc(ptr, new_size);
  if(ret_ptr == NULL){
    fprintf(stderr, "Error reallocating memory: %s\n", debug);
    exit(1);
  }
  return ret_ptr;
}
/* -------------------------------------------------- */
/* Self defined utility functions to make life easier */
/* -------------------------------------------------- */

//given an array and length, extract int from specified array segment
unsigned int extract_u_int(unsigned char *buf, int length){
  unsigned int retval = 0;
  for(int i = 0; i < length; i++)
    //note: values are represented in little endian
    retval += buf[i] << (i*8);
  return retval;
}

// signed variant
int extract_s_int(char *buf, int length){
  int retval = 0;
  for(int i = 0; i < length; i++)
    //note: values are represented in little endian
    retval += buf[i] << (i*8);
  return retval;
}

// traverse singly/doubly/triply indirect blocks to find all allocated block ptrs
// return value is an array of unsigned int arrays
// outer array has size of total_inodes_allocated
// inner array has size dependent on the filesystem

/* --------------------------- */
/* Functions to generate CSV's */
/* --------------------------- */

void write_superblock(superblock sb){
  int fd = Creat("super.csv", 0666, "creating superblock csv");
  FILE *fp = Fdopen(fd, "w", "opening superblock file stream");
  fprintf(fp, "%x,%d,%d,%d,%d,%d,%d,%d,%d\n", sb.magic_number, sb.total_inodes, sb.total_blocks,
	  sb.block_size, sb.fragment_size, sb.blocks_per_group, sb.inodes_per_group,
	  sb.fragments_per_group, sb.first_data_block);
  fclose(fp);
}

void write_group_descriptor(group_descriptor *all_gds){
  int fd = Creat("group.csv", 0666, "creating group descriptor csv");
  FILE *fp = Fdopen(fd, "w", "opening group descriptor file stream");
  for(int i = 0; i < total_block_groups; i++)
    fprintf(fp, "%d,%d,%d,%d,%x,%x,%x\n", all_gds[i].contained_blocks, all_gds[i].free_blocks,
	    all_gds[i].free_inodes, all_gds[i].num_directories, all_gds[i].inode_bitmap_block,
	    all_gds[i].block_bitmap_block, all_gds[i].inode_table_block);
  fclose(fp);
}

void write_free_bitmap_entry(free_bitmap_entry *all_fbes){
  int fd = Creat("bitmap.csv", 0666, "creating free bitmap entries csv");
  FILE *fp = Fdopen(fd, "w", "opening free bitmap entries file stream");
  for(int i = 0; i < total_free_entries; i++)
    fprintf(fp, "%x,%d\n", all_fbes[i].block_num, all_fbes[i].entry_num);
  fclose(fp);
}

void write_inode(inode *all_in){
  int fd = Creat("inode.csv", 0666, "creating inode csv");
  FILE *fp = Fdopen(fd, "w", "opening inode file stream");
  for(int i = 0; i < total_allocated_inodes; i++){
    fprintf(fp, "%d,%c,%o,%d,%d,%d,%x,%x,%x,%d,%d", all_in[i].inode_num, all_in[i].file_type,
	    all_in[i].mode, all_in[i].owner, all_in[i].group, all_in[i].link_count,
	    all_in[i].create_time, all_in[i].mod_time, all_in[i].access_time,
	    all_in[i].file_size, all_in[i].num_blocks);
    for(int j = 0; j < 15; j++)
      fprintf(fp, ",%x", all_in[i].block_ptrs[j]);
    fprintf(fp, "\n");
  }
  fclose(fp);
}

void write_directory_entry(directory_entry *all_des){
  int fd = Creat("directory.csv", 0666, "creating directory entries csv");
  FILE *fp = Fdopen(fd, "w", "opening directory entries file stream");
  for(int i = 0; i < num_dir_entries; i++){
    fprintf(fp, "%d,%d,%d,%d,%d,\"", all_des[i].parent_inode, all_des[i].entry_num,
	    all_des[i].entry_length, all_des[i].name_length, all_des[i].file_entry_inode_num);
    for(int j = 0; j < all_des[i].name_length; j++)
      fprintf(fp, "%c", all_des[i].name[j]);
    fprintf(fp, "\"\n");
  }
  fclose(fp);
}

void write_indirect_block_entry(indirect_block_entry *all_ides){
  int fd = Creat("indirect.csv", 0666, "creating indirect entries csv");
  FILE *fp = Fdopen(fd, "w", "opening indirect entries file stream");
  for(int i = 0; i < id_entry_count; i++){
    fprintf(fp, "%x,%d,%x\n", all_ides[i].block_num, all_ides[i].entry_num,
	    all_ides[i].block_ptr_val);
  }
  fclose(fp);
}


/* -------------------------------------------------- */
/* Functions to read from disk image and fill structs */
/* -------------------------------------------------- */


//TODO: Remove sanity checks?
/* SUPERBLOCK */
superblock get_superblock(int fs_fd){

  superblock fs_sb; //to return

  /* read superblock contents */
  unsigned char sb_buffer[1024];
  Pread(fs_fd, sb_buffer, 1024, 1024, "read superblock from file system");

  /* populate struct */
  fs_sb.magic_number       = extract_u_int(&sb_buffer[56], 2);
  fs_sb.total_inodes       = extract_u_int(&sb_buffer[0] , 4);
  fs_sb.total_blocks       = extract_u_int(&sb_buffer[4] , 4);
  fs_sb.blocks_per_group   = extract_u_int(&sb_buffer[32], 4);
  fs_sb.inodes_per_group   = extract_u_int(&sb_buffer[40], 4);
  fs_sb.fragments_per_group = extract_u_int(&sb_buffer[36], 4);
  fs_sb.first_data_block   = extract_u_int(&sb_buffer[20], 4);
  fs_sb.block_size         = 1024 << extract_u_int(&sb_buffer[24], 4);

  //appease signedness
  char signed_buf[4];
  for(int i = 0; i < 4; i++)
    signed_buf[i] = sb_buffer[28+i];
  if(extract_s_int(signed_buf, 4) > 0)
    fs_sb.fragment_size	   = 1024 << extract_s_int(signed_buf, 4);
  else
    fs_sb.fragment_size	   = 1024 >> -extract_s_int(signed_buf, 4);

  
  /* extras */
  file_system_size = (unsigned long long) (fs_sb.total_blocks * fs_sb.block_size);
  total_blocks = fs_sb.total_blocks;
  total_inodes = fs_sb.total_inodes;
  block_size = fs_sb.block_size;
  blocks_per_group = fs_sb.blocks_per_group;
  inodes_per_group = fs_sb.inodes_per_group;
  
  /* SANITY CHECKING */
  //Magic number must be correct
  if(fs_sb.magic_number != EXT2_SUPER_MAGIC){
    fprintf(stderr, "Superblock - invalid magic: %x\n", fs_sb.magic_number);
    exit(1);
  }

  //Block size must be reasonable (e.g. power of two between 512-64K)
  if(fs_sb.block_size < 512 || fs_sb.block_size > 64 * 1024){
    fprintf(stderr, "Superblock - invalid block size: %d\n", fs_sb.block_size);
    exit(1);
  }
  unsigned int temp = fs_sb.block_size;
  while(temp > 1){
    int mod = temp % 2;
    if(mod != 0){
      fprintf(stderr, "Superblock - invalid block size: %d\n", fs_sb.block_size);
      exit(1);
    }
    temp /= 2;
  }

  //Total blocks and first data block must be consistent with the file size
  struct stat s;
  fstat(fs_fd, &s);
  unsigned long long total_size = (unsigned long long) s.st_size;
  if(fs_sb.total_blocks > total_size){
    fprintf(stderr, "Superblock - invalid block count %d > image size %llu\n",
	    fs_sb.total_blocks, total_size);
    exit(1);
  }
  if(fs_sb.first_data_block > total_size){
    fprintf(stderr, "Superblock - invalid first block %d > image size %llu\n",
	    fs_sb.first_data_block, total_size);
    exit(1);
  }

  //Blocks per group must evenly divide into total blocks
  if(fs_sb.total_blocks % fs_sb.blocks_per_group != 0){
    fprintf(stderr, "Superblock - %d blocks, %d blocks/group\n",
	    fs_sb.total_blocks, fs_sb.blocks_per_group);
    exit(1);
  }

  //Inodes per group must evenly divide into total inodes
  if(fs_sb.total_inodes % fs_sb.inodes_per_group != 0){
    fprintf(stderr, "Superblock - %d inodes, %d inodes/group\n",
	    fs_sb.total_inodes, fs_sb.inodes_per_group);
    exit(1);
  }
  /* END SANITY CHECKING */


  return fs_sb;
}
/* END SUPERBLOCK */

/* -------------------------------------------- */

/* GROUP DESCRIPTOR */
group_descriptor *get_group_descriptors(int fs_fd){
  group_descriptor *all_gds;
  total_block_groups = total_blocks / blocks_per_group;
  all_gds = (group_descriptor *)Malloc(total_block_groups*sizeof(group_descriptor),
				       "allocate all group descriptors");
  for(int i = 0; i < total_block_groups; i++){
    unsigned char buf[EXT2_GROUP_DES_SIZE];
    Pread(fs_fd, buf, EXT2_GROUP_DES_SIZE, 2048 + i * EXT2_GROUP_DES_SIZE,
	  "reading individual group descriptor entry");

    if(i == total_block_groups-1)
      all_gds[i].contained_blocks = total_blocks - blocks_per_group * i; 
    else
      all_gds[i].contained_blocks = blocks_per_group;

    all_gds[i].free_blocks = extract_u_int(&buf[12], 2);
    all_gds[i].free_inodes = extract_u_int(&buf[14], 2);
    all_gds[i].num_directories = extract_u_int(&buf[16], 2);
    all_gds[i].inode_bitmap_block = extract_u_int(&buf[4], 4);
    all_gds[i].block_bitmap_block = extract_u_int(&buf[0], 4);
    all_gds[i].inode_table_block = extract_u_int(&buf[8], 4);
  }
  
  return all_gds;
}
/* END GROUP DESCRIPTOR */

/* --------------------------------------------- */

/* FREE BITMAP ENTRY */
free_bitmap_entry *get_free_bitmap_entries(int fs_fd, group_descriptor *fs_all_gds){
  free_bitmap_entry *fs_fbes;

  // each bitmap contains block_size bytes
  // two bitmaps (block and inode) per group, for a total of 2 * total_block_group
  unsigned int total_bytes = block_size * 2 * total_block_groups;

  // hold all the bytes for the bitmaps
  unsigned char *bytes = (unsigned char*)Malloc(total_bytes*sizeof(unsigned char),
						"allocating memory for bitmap bytes");
  // hold all the bits for every byte
  unsigned char *bits = (unsigned char*)Malloc(8*total_bytes*sizeof(unsigned char),
					       "allocating memory for bitmap bits");  
  // read all the bytes
  for(int i = 0; i < total_block_groups; i++)
    Pread(fs_fd, &bytes[i * 2 * block_size], 2 * block_size,
	  fs_all_gds[i].block_bitmap_block * block_size,
	  "reading block and inode bitmaps");

  unsigned char bitmask = 1;
  // populate bits array using bytes array
  for(int i = 0; i < total_bytes; i++){
    for(int j = 0; j < 8; j++){
      bits[i*8 + j] = (bytes[i] & (bitmask << j)) != 0;
      if(bits[i*8 + j] == 0)
	total_free_entries++;
    }
  }
  
  // allocate appropriate number of bitmap entries
  fs_fbes = (free_bitmap_entry *)Malloc(total_free_entries*sizeof(free_bitmap_entry),
					"allocating free bitmap entries");

  /* begin populating structs */
  
  unsigned int curr_block_entry = 0;
  unsigned int curr_inode_entry = 0;
  unsigned int curr_free = 0;
  for(int i = 0; i < total_block_groups*2; i++){
  
    // determine the block/inode bitmap id
    int is_inode = i%2;
    unsigned int curr_block_num = fs_all_gds[i/2].block_bitmap_block;
    if(is_inode == 1)
      curr_block_num = fs_all_gds[i/2].inode_bitmap_block;

    //keep track of how many blocks/inodes traversed for this particular group
    unsigned int blocks_encountered = 0;
    unsigned int inodes_encountered = 0;
    
    // iterate through all the bytes in each bitmap
    for(int j = 0; j < block_size; j++){

      //avoid traversing meaningless bytes (also fixes problem of skewing inode number)
      if(blocks_encountered >= blocks_per_group)
	break;
      if(inodes_encountered >= inodes_per_group)
	break;
      
      //iterate through each bit in a particular byte, populating free entry struct if bit is 0
      for(int k = 0; k < 8; k++){

	//keep track of which # block or inode we're on
	if(is_inode == 1)
	  curr_inode_entry++;
	else
	  curr_block_entry++;

	//found free entry, populate struct
	if(bits[i*block_size*8 + j*8 + k] == 0){
	  fs_fbes[curr_free].block_num = curr_block_num;
	  if(is_inode == 1)
	    fs_fbes[curr_free].entry_num = curr_inode_entry;
	  else
	    fs_fbes[curr_free].entry_num = curr_block_entry;
	  curr_free++;
	}
	else if(bits[i*block_size*8 + j*8 + k] == 1 && is_inode == 1)
	  total_allocated_inodes++;

	//keep track of how many blocks/inode we've encountered in this bitmap
	if(is_inode == 1)
	  inodes_encountered++;
	else
	  blocks_encountered++;

      }
    }
  }

  free(bytes);
  free(bits);
  
  return fs_fbes;
}
/* END FREE BITMAP ENTRY */


/* START INODE */

/* Pseudocode
 * For each block group, find the inode bitmap
 * Iterate through inode bitmap, and for each allocated inode in:
 * Jump to inode table, index to the location of in, and read data into struct
 */

inode *get_inodes(int fs_fd, group_descriptor *fs_all_gds){
  inode *fs_inodes;
  unsigned char **inode_bitmaps;
  unsigned char *buf;

  unsigned int inode_bitmap_bytesize = inodes_per_group / 8;
  unsigned int total_bytes = inode_bitmap_bytesize * total_block_groups;

  //allocate memory for inode array, inode bitmaps, and inode bitmap buffer
  fs_inodes = (inode *)Malloc(total_allocated_inodes*sizeof(inode),
			      "allocate inodes");
  inode_bitmaps = (unsigned char **)Malloc(total_block_groups*sizeof(unsigned char *),
					   "allocate inode bitmap for each group");
  for(int i = 0; i < total_block_groups; i++)
    inode_bitmaps[i] = (unsigned char *)Malloc(inodes_per_group*sizeof(unsigned char),
					       "allocate space for bitmap entries");
  buf =	(unsigned char *)Malloc(total_bytes*sizeof(unsigned char),
			       "allocate buffer for inode bitmaps");
  
  // read in the inode bitmap to the buffer
  for(int i = 0; i < total_block_groups; i++)
    Pread(fs_fd, &buf[i * inode_bitmap_bytesize], inode_bitmap_bytesize,
	  fs_all_gds[i].inode_bitmap_block * block_size, "reading inode bitmaps");

  unsigned char bitmask = 1;
  // populate bitmap array using the buffer (1 byte --> 8 bits)
  for(int i = 0; i < total_block_groups; i++){
    
    unsigned char *curr_bitmap = inode_bitmaps[i];
    
    for(int j = 0; j < inode_bitmap_bytesize; j++){
      for(int k = 0; k < 8; k++){
	curr_bitmap[j*8 + k] = (buf[i * inode_bitmap_bytesize + j] & (bitmask << k)) != 0;
      }
    }
  }

  // begin populating structs
  unsigned int curr_inode = 0;
  for(int i = 0; i < total_block_groups; i++){

    // remember the offset is this * block_size
    unsigned int curr_table_start = fs_all_gds[i].inode_table_block;

    unsigned char *curr_bitmap = inode_bitmaps[i];
    
    for(int j = 0; j < inodes_per_group; j++){
      if(curr_bitmap[j] == 1){

	unsigned char inode_buf[EXT2_INODE_ENTRY_SIZE];
	
	Pread(fs_fd, inode_buf, EXT2_INODE_ENTRY_SIZE,
	      curr_table_start * block_size + j * EXT2_INODE_ENTRY_SIZE,
	      "reading the contents of 1 allocated inode");

	fs_inodes[curr_inode].inode_num = i * inodes_per_group + j + 1;

	unsigned int file_type = extract_u_int(&inode_buf[0], 2) >> 12;
	switch(file_type){
	case EXT2_FILETYPE_FILE:
	  fs_inodes[curr_inode].file_type = 'f';
	  break;
	case EXT2_FILETYPE_DIRECTORY:
	  fs_inodes[curr_inode].file_type = 'd';
	  break;
	case EXT2_FILETYPE_SYMLINK:
	  fs_inodes[curr_inode].file_type = 's';
	  break;
	default:
	  fs_inodes[curr_inode].file_type = '?';
	  break;
	}

	fs_inodes[curr_inode].mode = extract_u_int(&inode_buf[0], 2);
	fs_inodes[curr_inode].owner = extract_u_int(&inode_buf[2], 2);
	fs_inodes[curr_inode].group = extract_u_int(&inode_buf[24], 2);
	fs_inodes[curr_inode].link_count = extract_u_int(&inode_buf[26], 2);
	fs_inodes[curr_inode].create_time = extract_u_int(&inode_buf[12], 4);
	fs_inodes[curr_inode].mod_time = extract_u_int(&inode_buf[16], 4);
	fs_inodes[curr_inode].access_time = extract_u_int(&inode_buf[8], 4);
	fs_inodes[curr_inode].file_size = extract_u_int(&inode_buf[4], 4);
	fs_inodes[curr_inode].num_blocks = extract_u_int(&inode_buf[28], 4) / (block_size/512);

	for(int p = 0; p < 15; p++){
	  fs_inodes[curr_inode].block_ptrs[p] = extract_u_int(&inode_buf[40 + 4*p], 4);
	}

	curr_inode++;
      }
    }
  }

  for(int i = 0; i < total_block_groups; i++)
    free(inode_bitmaps[i]);
  free(inode_bitmaps);
  free(buf);
  return fs_inodes;
}
/* END INODE */


/* START DIRECTORY ENTRY */

/* Pseudocode
 * Iterate through array of inodes
 * Check each inode, if it is a directory:
 * Its inode number is the parent inode number
 * Generate array of data block pointers (including those in indirect blocks)
 * Go through inode's data blocks and print directory entries
 * 
 */

directory_entry *get_directory_entries(int fs_fd, inode* fs_inodes){

  directory_entry *fs_dir_entries;

  unsigned int *all_datablock_ptrs[total_allocated_inodes];
  
  unsigned int curr_dir_entry = 0;


  /* find all datablock ptrs, populate all_datablock_ptrs */
  for(int i = 0; i < total_allocated_inodes; i++){    
    // skip inodes that aren't directories
    if(fs_inodes[i].file_type != 'd')
      continue;


    // we have the parent inode, look through its data blocks
    inode parent_inode = fs_inodes[i];
    //unsigned int parent_inode_num = parent_inode.inode_num;
    unsigned int num_datablocks_used = parent_inode.num_blocks;
    
    unsigned int *datablock_ptrs;
    datablock_ptrs = (unsigned int*)Malloc(num_datablocks_used*sizeof(unsigned int),
					   "allocating space for directory datablock ptrs");
    if(num_datablocks_used > 12){
      // please please please no ...

      /* traverse triply, doubly, and singly indirect blocks for all ptrs */

      unsigned int *doubly_indirect_block_ptrs;
      unsigned int *indirect_block_ptrs;
      unsigned int num_doubly_ptrs = 0;
      unsigned int num_indirect_ptrs = 0;

      if(parent_inode.block_ptrs[13] != 0)
	num_doubly_ptrs++;

      if(parent_inode.block_ptrs[12] != 0)
	num_indirect_ptrs++;

      /* -------------------------------------------------------------------- */
      /* read through triply indirect block and find all doubly indirect ptrs */
      /* -------------------------------------------------------------------- */
      
      // find number of doubly indirect pointers in triply indirect block
      if(parent_inode.block_ptrs[14] != 0){

	unsigned char triply_buf[block_size];
	Pread(fs_fd, triply_buf, block_size, parent_inode.block_ptrs[14]*block_size,
	      "reading triply indirect block");

	unsigned int index = 0;
	while(index < block_size){
	  unsigned int found_ptr = extract_u_int(&triply_buf[index], 4);

	  if(found_ptr == 0)
	    break;

	  num_doubly_ptrs++;
	  index += 4;
	}
      }

      // allocate array of doubly indirect ptrs
      if(num_doubly_ptrs > 0){
	doubly_indirect_block_ptrs = (unsigned int*)Malloc(num_doubly_ptrs*sizeof(unsigned int*),
							   "allocating doubly indirect ptrs");
	unsigned int array_index = 0;
	// add doubly indirect ptrs to array
	if(parent_inode.block_ptrs[13] != 0){
	  doubly_indirect_block_ptrs[0] = parent_inode.block_ptrs[13];
	  array_index++;
	}

	if(parent_inode.block_ptrs[14] != 0){

	  unsigned char triply_buf[block_size];
	  Pread(fs_fd, triply_buf, block_size, parent_inode.block_ptrs[14]*block_size,
		"reading triply indirect block");

	  unsigned int index = 0;
	  
	  while(index < block_size){
	    unsigned int found_ptr = extract_u_int(&triply_buf[index], 4);

	    if(found_ptr == 0)
	      break;

	    doubly_indirect_block_ptrs[array_index] = found_ptr;
	    array_index++;
	    index += 4;
	  }
	}
      }
      else
	doubly_indirect_block_ptrs = NULL;

      /* -------------------------------------------------------------- */
      /* read through doubly indirect blocks and find all indirect ptrs */
      /* -------------------------------------------------------------- */

      // look through all doubly indirect blocks for number of singly indirect ptrs
      for(int j = 0; j < num_doubly_ptrs; j++){

	unsigned char doubly_buf[block_size];
	Pread(fs_fd, doubly_buf, block_size, doubly_indirect_block_ptrs[j]*block_size,
	      "reading doubly indirect block");

	unsigned int index = 0;
	while(index < block_size){
	  unsigned int found_ptr = extract_u_int(&doubly_buf[index], 4);

	  if(found_ptr == 0)
	    break;

	  num_indirect_ptrs++;
	  index += 4;
	}
      }

      // allocate array of indirect ptrs
      if(num_indirect_ptrs > 0){
	indirect_block_ptrs = (unsigned int*)Malloc(num_indirect_ptrs*sizeof(unsigned int),
						    "allocate indirect ptrs");
	unsigned int array_index = 0;
	// add indirect ptrs to array
	if(parent_inode.block_ptrs[12] != 0){
	  indirect_block_ptrs[0] = parent_inode.block_ptrs[12];
	  array_index++;
	}

	for(int j = 0; j < num_doubly_ptrs; j++){

	  unsigned char doubly_buf[block_size];
	  Pread(fs_fd, doubly_buf, block_size, doubly_indirect_block_ptrs[j]*block_size,
		"reading doubly indirect block");

	  unsigned int index = 0;
	  while(index < block_size){
	    unsigned int found_ptr = extract_u_int(&doubly_buf[index], 4);

	    if(found_ptr == 0)
	      break;

	    indirect_block_ptrs[array_index] = found_ptr;
	    array_index++;
	    index += 4;
	  }
	}
      }
      else
	indirect_block_ptrs = NULL;      

      
      /* Populate datablock ptrs 
       * add the first 12
       * read through indirect blocks and find all block ptrs */

      unsigned int array_index = 0;
      // deal with the first 12 datablocks
      for(int j = 0; j < 12; j++){
	if(parent_inode.block_ptrs[j] != 0){
	  datablock_ptrs[array_index] = parent_inode.block_ptrs[j];
	  array_index++;
	}
      }
      
      // read through indirect blocks and find all block ptrs
      for(int j = 0; j < num_indirect_ptrs; j++){
	unsigned char indirect_buf[block_size];
	Pread(fs_fd, indirect_buf, block_size, indirect_block_ptrs[j]*block_size,
	      "reading indirect block");

	unsigned int index = 0;

	while(index < block_size){

	  unsigned int found_ptr = extract_u_int(&indirect_buf[index], 4);

	  if(found_ptr == 0)
	    break;

	  datablock_ptrs[array_index] = found_ptr;
	  array_index++;
	  index += 4;
	  
	}		
      }
      
      if(array_index > num_datablocks_used){
	fprintf(stderr,"likely corrupted file system with %d blocks allocated, but inode only reports %d blocks", array_index, num_datablocks_used);
	exit(1);
      }
            
    }
    else
      for(int j = 0; j < num_datablocks_used; j++)
	datablock_ptrs[j] = parent_inode.block_ptrs[j];

    all_datablock_ptrs[i] = datablock_ptrs;
  }
    
  /* find total number of directory entries so we can allocate enough space for our result */
  for(int i = 0; i < total_allocated_inodes; i++){

    // skip inodes that aren't directories
    if(fs_inodes[i].file_type != 'd')
      continue;

    // we have the parent inode, look through its data blocks
    inode parent_inode = fs_inodes[i];
    unsigned int num_datablocks_used = parent_inode.num_blocks;
    
    /* traverse each data block, first to get the number of directory entries, then to record */

    // get # of directory entries for this inode
    for(int j = 0; j < num_datablocks_used; j++){

      unsigned int *datablock_ptrs = all_datablock_ptrs[i];
      
      unsigned char block_buf[block_size];

      if(datablock_ptrs[j] == 0)
	continue;
      
      Pread(fs_fd, block_buf, block_size, datablock_ptrs[j] * block_size,
	    "read directory datablock into buffer");
      
      unsigned int index = 0;
      while(index < block_size){

	unsigned int entry_inode_num = extract_u_int(&block_buf[index], 4);
	unsigned int entry_rec_len = extract_u_int(&block_buf[index + 4], 2);

	if(entry_rec_len == 0){
	  fprintf(stderr, "you've made a grave mistake");
	  exit(1);
	}
	
	if(entry_inode_num != 0)
	  num_dir_entries++;
	index += entry_rec_len;
      }
    }
  }

  
  /* allocate return struct */
  fs_dir_entries = (directory_entry *)Malloc(num_dir_entries*sizeof(directory_entry),
					     "allocation of directory entries");
  
  /* populate our return struct using the datablocks */
  for(int i = 0; i < total_allocated_inodes; i++){
    
    // skip inodes that aren't directories
    if(fs_inodes[i].file_type != 'd')
      continue;

    // we have the parent inode, look through its data blocks
    inode parent_inode = fs_inodes[i];
    unsigned int parent_inode_num = parent_inode.inode_num;
    unsigned int num_datablocks_used = parent_inode.num_blocks;

    unsigned curr_inode_entry_num = 0;
    // populate struct with this inode's directory entries
    for(int j = 0; j < num_datablocks_used; j++){

      unsigned int *datablock_ptrs = all_datablock_ptrs[i];
      
      unsigned char block_buf[block_size];

      if(datablock_ptrs[j] == 0)
        continue;
      
      Pread(fs_fd, block_buf, block_size, datablock_ptrs[j] * block_size,
            "read directory datablock into buffer");
      unsigned int index = 0;
      while(index < block_size){

	unsigned int entry_inode_num = extract_u_int(&block_buf[index], 4);
	unsigned int entry_rec_len = extract_u_int(&block_buf[index + 4], 2);

	if(entry_inode_num == 0){
	  index += entry_rec_len;
	  curr_inode_entry_num++;
	  continue;
	}

	unsigned int name_len = block_buf[index + 6];
	
	fs_dir_entries[curr_dir_entry].parent_inode = parent_inode_num;
	fs_dir_entries[curr_dir_entry].entry_num = curr_inode_entry_num;
	fs_dir_entries[curr_dir_entry].entry_length = entry_rec_len;
	fs_dir_entries[curr_dir_entry].name_length = name_len;
	fs_dir_entries[curr_dir_entry].file_entry_inode_num = entry_inode_num;
	fs_dir_entries[curr_dir_entry].name = (char *)Malloc(name_len*sizeof(char),
							     "allocate space for name");
	for(int i = 0; i < name_len; i++)
	  fs_dir_entries[curr_dir_entry].name[i] = block_buf[index + 8 + i];

	index += entry_rec_len;
	curr_inode_entry_num++;
	curr_dir_entry++;
      } 
    }
    
  }

  return fs_dir_entries;
}

/* END DIRECTORY ENTRY */


/* START INDIRECT BLOCK ENTRIES */

indirect_block_entry *get_indirect_block_entries(int fs_fd, inode* fs_inodes){

  indirect_block_entry *fs_id_entries;
  unsigned int id_entries_index = 0;
  unsigned int return_array_allocated = 0;
  
  for(int i = 0; i < total_allocated_inodes; i++){

    inode parent_inode = fs_inodes[i];
    unsigned int inode_num_blocks = parent_inode.num_blocks;
    
    unsigned int *doubly_indirect_block_ptrs;
    unsigned int *indirect_block_ptrs;
    unsigned int num_doubly_ptrs = 0;
    unsigned int num_indirect_ptrs = 0;

    // check parent inode for doubly indirect pointer
    if(parent_inode.block_ptrs[13] != 0 && inode_num_blocks >= 14){
      num_doubly_ptrs++;
    }

    // check parent inode for singly indirect pointer
    if(parent_inode.block_ptrs[12] != 0 && inode_num_blocks >= 13){
      num_indirect_ptrs++;
    }

    /* -------------------------------------------------------------------- */
    /* read through triply indirect block and find all doubly indirect ptrs */
    /* -------------------------------------------------------------------- */

    // find number of doubly indirect pointers in triply indirect block
    if(parent_inode.block_ptrs[14] != 0){

      unsigned char triply_buf[block_size];
      Pread(fs_fd, triply_buf, block_size, parent_inode.block_ptrs[14]*block_size,
	    "read triply indirect block for part 6");

      unsigned int index = 0;
      while(index < block_size){
	unsigned int found_ptr = extract_u_int(&triply_buf[index], 4);

	if(found_ptr == 0)
	  break;

	num_doubly_ptrs++;
	id_entry_count++;
	index += 4;
      }
    }
    
    // allocate & store array of doubly indirect ptrs
    // populate return array with found entries from triply indirect block
    if(num_doubly_ptrs > 0){
      doubly_indirect_block_ptrs = (unsigned int *)Malloc(num_doubly_ptrs*sizeof(unsigned int *),
							  "allocating doubly indirect ptrs for 6");
      if(return_array_allocated == 0){
	fs_id_entries = (indirect_block_entry *)Malloc(id_entry_count*sizeof(indirect_block_entry),
						     "allocating return array");
	return_array_allocated = 1;
      }
      else
	fs_id_entries = (indirect_block_entry *)Realloc(fs_id_entries,
							id_entry_count*sizeof(indirect_block_entry),
							"reallocating return array"); 
      unsigned int array_index = 0;

      // add the inode's doubly indirect ptr if available
      if(parent_inode.block_ptrs[13] != 0 && inode_num_blocks >= 14){
	doubly_indirect_block_ptrs[0] = parent_inode.block_ptrs[13];
	array_index++;
      }

      // look through triply indirect block to add ptrs and add entries
      if(parent_inode.block_ptrs[14] != 0){

	unsigned char triply_buf[block_size];
	Pread(fs_fd, triply_buf, block_size, parent_inode.block_ptrs[14]*block_size,
	      "reading triply indirect block for 6");

	unsigned int index = 0; //buffer index

	while(index < block_size){
	  unsigned int found_ptr = extract_u_int(&triply_buf[index], 4);

	  if(found_ptr == 0)
	    break;

	  doubly_indirect_block_ptrs[array_index] = found_ptr;
	  array_index++;

	  fs_id_entries[id_entries_index].block_num = parent_inode.block_ptrs[14];
	  fs_id_entries[id_entries_index].entry_num = index/4;
	  fs_id_entries[id_entries_index].block_ptr_val = found_ptr;
	  id_entries_index++;

	  index += 4;
	}
      }
    }
    else
      doubly_indirect_block_ptrs = NULL;

    /* ------------------------------------------------------------- */
    /* read through doubly indirect block and find all indirect ptrs */
    /* ------------------------------------------------------------- */

    // look through doubly indirect blocks for number of singly indirect ptrs
    for(int j = 0; j < num_doubly_ptrs; j++){

      unsigned char doubly_buf[block_size];
      Pread(fs_fd, doubly_buf, block_size, doubly_indirect_block_ptrs[j]*block_size,
	    "reading doubly indirect block for 6");

      unsigned int index = 0;
      while(index < block_size){
	unsigned int found_ptr = extract_u_int(&doubly_buf[index], 4);

	if(found_ptr == 0)
	  break;

	num_indirect_ptrs++;
	id_entry_count++;
	index += 4;
      }
    }

    // allocate and store array of indirect ptrs
    // populate return array with found entries from doubly indirect blocks
    if(num_indirect_ptrs > 0){
      indirect_block_ptrs = (unsigned int *)Malloc(num_indirect_ptrs*sizeof(unsigned int),
						   "allocate indirect ptrs for 6");
      if(return_array_allocated == 0){
	fs_id_entries = (indirect_block_entry *)Malloc(id_entry_count*sizeof(indirect_block_entry),
                                                     "allocating return array");
	return_array_allocated = 1;
      }
      else
        fs_id_entries = (indirect_block_entry *)Realloc(fs_id_entries,
                                                        id_entry_count*sizeof(indirect_block_entry),
                                                        "reallocating return array");
      unsigned int array_index = 0;
      
      // add inode's singly indirect ptr if available
      if(parent_inode.block_ptrs[12] != 0 && inode_num_blocks >= 13){
	indirect_block_ptrs[0] = parent_inode.block_ptrs[12];
	array_index++;
      }

      // look through doubly indirect blocks to add ptrs and add entries
      for(int j = 0; j < num_doubly_ptrs; j++){

	unsigned char doubly_buf[block_size];
	Pread(fs_fd, doubly_buf, block_size, doubly_indirect_block_ptrs[j]*block_size,
	      "reading doubly indirect block for 6");

	unsigned int index = 0; // buffer index
	while(index < block_size){
	  unsigned int found_ptr = extract_u_int(&doubly_buf[index], 4);

	  if(found_ptr == 0)
	    break;

	  indirect_block_ptrs[array_index] = found_ptr;
	  array_index++;

	  fs_id_entries[id_entries_index].block_num = doubly_indirect_block_ptrs[j];
	  fs_id_entries[id_entries_index].entry_num = index/4;
	  fs_id_entries[id_entries_index].block_ptr_val = found_ptr;
	  id_entries_index++;
	  
	  index += 4;
	}
      }
    }
    else
      indirect_block_ptrs = NULL;

    /* ----------------------------------------------------- */
    /* read through singly indirect blocks and find all ptrs */
    /* ----------------------------------------------------- */

    unsigned int singly_indirect_entries = 0;
    // find number of entries
    for(int j = 0; j < num_indirect_ptrs; j++){

      unsigned char indirect_buf[block_size];
      Pread(fs_fd, indirect_buf, block_size, indirect_block_ptrs[j]*block_size,
	    "reading indirect block for 6");

      unsigned int index = 0;

      while(index < block_size){
	unsigned int found_ptr = extract_u_int(&indirect_buf[index], 4);

	if(found_ptr == 0)
	  break;

	singly_indirect_entries++;
	id_entry_count++;
        index += 4;
      }
    }

    // populate return array with entries found in singly indirect blocks
    if(singly_indirect_entries > 0){

      if(return_array_allocated == 0){
        fs_id_entries = (indirect_block_entry *)Malloc(id_entry_count*sizeof(indirect_block_entry),
                                                     "allocating return array");
        return_array_allocated = 1;
      }
      else
        fs_id_entries = (indirect_block_entry *)Realloc(fs_id_entries,
                                                        id_entry_count*sizeof(indirect_block_entry),
                                                        "reallocating return array");
      for(int j = 0; j < num_indirect_ptrs; j++){

	unsigned char indirect_buf[block_size];
	Pread(fs_fd, indirect_buf, block_size, indirect_block_ptrs[j]*block_size,
	      "reading indirect block for 6");

	unsigned int index = 0;
	while(index < block_size){
	  unsigned int found_ptr = extract_u_int(&indirect_buf[index], 4);

	  if(found_ptr == 0)
	    break;
	  
	  fs_id_entries[id_entries_index].block_num = indirect_block_ptrs[j];
          fs_id_entries[id_entries_index].entry_num = index/4;
          fs_id_entries[id_entries_index].block_ptr_val = found_ptr;
          id_entries_index++;

          index += 4;
	}
      }
    }
  }

  return fs_id_entries;
  
}


/* END INDIRECT BLOCK ENTRIES */
