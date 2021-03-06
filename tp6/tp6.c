#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tp6.h"
#include "include/hardware.h"
#include "dump.h"

#define DISK_SECT_SIZE_MASK 0x0000FF
#define MAX_VOL 8
#define MBR_MAGIC 0xCAFE

enum vd_type_e {VT_BASE, VT_ANX, VT_OTH};

struct vd_descr_s{
	unsigned vd_first_sector;
	unsigned vd_first_cylinder;
	unsigned vd_n_sector;
	enum vd_type_e vd_val_type;
};

struct mbr_s{
	unsigned mbr_nvol;
	struct vd_descr_s mbr_vols[MAX_VOL];
	unsigned mbr_magic;
};

struct mbr_s mbr;

int create_new_volume(unsigned f_s, unsigned f_c, unsigned n_s, enum vd_type_e type){
	if(mbr.mbr_nvol == MAX_VOL){
		fprintf(stderr, "Max number of volumes reached\n");
		return -1;
	}
	struct vd_descr_s new_vol;
	new_vol.vd_first_sector = f_s;
	new_vol.vd_first_cylinder = f_c;
	new_vol.vd_n_sector = n_s;
	new_vol.vd_val_type = type;
	
	mbr.mbr_vols[mbr.mbr_nvol] = new_vol;
	printf("%d, %d,  %d\n\n",f_c,f_s,n_s);
	
	printf_vol(mbr.mbr_nvol);
	return mbr.mbr_nvol++;
}

enum vd_type_e retrieve_type_from_string(char *buffer){
	if (!strcmp(buffer, "VT_BASE")) return VT_BASE;
	if (!strcmp(buffer, "VT_ANX")) return VT_ANX;
	if (!strcmp(buffer, "VT_OTH")) return VT_OTH;
	return -1;
}

int interactive_create_new_volume(){
	unsigned f_s;
	unsigned f_c;
	unsigned n_s;
	enum vd_type_e type;
	char buffer[1024];

	printf("Please enter the first cylinder:\n");
	if(scanf("%i",&f_c) == 0) return -1;
	printf("Please enter the first sector:\n");
	if(scanf("%i",&f_s) == 0) return -1;
	printf("Please enter the number of sectors:\n");
	if(scanf("%i",&n_s) == 0) return -1;
	printf("Please enter the volume type (VT_BASE, VT_ANX, VT_OTH):\n");
	if(scanf("%s",buffer) == 0) return -1;
	
	type = retrieve_type_from_string(buffer);
	
	return create_new_volume(f_s, f_c, n_s, type);
}

int delete_volume(unsigned int vol){
	int i;
	if (vol<0 || vol >= mbr.mbr_nvol){
		fprintf(stderr, "Can't delete this volume, the index is wrong\n");
		return 1;
	}
	for ( i = vol+1 ; i < mbr.mbr_nvol; i ++){
		mbr.mbr_vols[i-1] = mbr.mbr_vols[i];
	}
	mbr.mbr_nvol--;	
	return 0;
}

int interactive_delete_volume(){
	unsigned int vol;
	
	printf("Please enter the volume index to remove:\n");
	if(scanf("%i",&vol) == 0) return 1;
	return delete_volume(vol);
}

static void empty_it(){
    return;
}

void gotoSector(int cyl, int sect){
	if(cyl<0 || cyl > HDA_MAXCYLINDER){
		fprintf(stderr, "Wrong cylinder index\n");
		exit(EXIT_FAILURE);
	}
	
	if(sect<0 || sect > HDA_MAXSECTOR){
		fprintf(stderr, "Wrong sector index\n");
		exit(EXIT_FAILURE);
	}
	
	_out(HDA_DATAREGS, (cyl>>8) & 0xFF);
	_out(HDA_DATAREGS+1, cyl & 0xFF);
	_out(HDA_DATAREGS+2, (sect>>8) & 0xFF);
	_out(HDA_DATAREGS+3, sect & 0xFF);
	_out(HDA_CMDREG, CMD_SEEK);
	_sleep(HDA_IRQ);
}

void read_sector_n(unsigned int cyl, unsigned int sect, unsigned char *buffer, int size){
	int i;
	if(cyl<0 || cyl > HDA_MAXCYLINDER){
		fprintf(stderr, "Wrong cylinder index\n");
		exit(EXIT_FAILURE);
	}
	if(sect<0 || sect > HDA_MAXSECTOR){
		fprintf(stderr, "Wrong sector index\n");
		exit(EXIT_FAILURE);
	}
	
	if(size<0 || size > HDA_SECTORSIZE){
		fprintf(stderr, "Wrong size\n");
		exit(EXIT_FAILURE);
	}
	
	gotoSector(cyl, sect);
	_out(HDA_DATAREGS, 0);
	_out(HDA_DATAREGS+1, 1);
	_out(HDA_CMDREG, CMD_READ);
	_sleep(HDA_IRQ);
	for (i = 0; i < size ; i ++){
		buffer[i] = MASTERBUFFER[i];
	}
}

void read_sector(unsigned int cyl, unsigned int sect, unsigned char *buffer){
	read_sector_n(cyl, sect, buffer, HDA_SECTORSIZE);
}

void write_sector(unsigned int cyl, unsigned int sect, const unsigned char *buffer){
	int i;
	if(cyl<0 || cyl > HDA_MAXCYLINDER){
		fprintf(stderr, "Wrong cylinder index\n");
		exit(EXIT_FAILURE);
	}
	if(sect<0 || sect > HDA_MAXSECTOR){
		fprintf(stderr, "Wrong sector index\n");
		exit(EXIT_FAILURE);
	}
	
	gotoSector(cyl, sect);
	
	for (i = 0; i < HDA_SECTORSIZE ; i ++){
		MASTERBUFFER[i] = buffer[i];
	}
	
	_out(HDA_DATAREGS, 0);
	_out(HDA_DATAREGS+1, 1);
	_out(HDA_CMDREG, CMD_WRITE);
	_sleep(HDA_IRQ);
}

void format(){
	int i;
	
	for(i = 0 ; i < HDA_MAXCYLINDER; i ++){
		format_sector(i, 0, HDA_MAXSECTOR, 0);
	}
}

void format_sector(unsigned int cylinder, unsigned int sector, unsigned int nsector, unsigned int value){
	int i;
	unsigned char buffer[HDA_SECTORSIZE];
	
	for (i = 0; i < HDA_SECTORSIZE ; i ++){
		buffer[i] = value;
	}
	
	// if((sector+nsector) > HDA_MAXSECTOR){
	// 	fprintf(stderr, "Too much sectors to format\n");
	// 	exit(EXIT_FAILURE);
	// }
	
	for(i = sector ; i < (sector + nsector) ; i++){
		write_sector(cylinder,i,buffer);
	}
}

int load_mbr(){
	read_sector_n(0,0, ((unsigned char*)(&mbr)), sizeof(struct mbr_s));
	if(mbr.mbr_magic == MBR_MAGIC){
		return 0;
	}
	mbr.mbr_nvol = 0;
	mbr.mbr_magic = MBR_MAGIC;
	return 1;
}

void save_mbr(){
	write_sector(0, 0, ((unsigned char*)(&mbr)));
}

unsigned cylinder_of_bloc(int vol, int n_bloc){
	if (vol<0 || vol > mbr.mbr_nvol){
		fprintf(stderr, "Wrong volume number\n");
		exit(EXIT_FAILURE);
	}
	if (n_bloc<0 || vol > mbr.mbr_vols[vol].vd_n_sector){
		fprintf(stderr, "Wrong bloc number\n");
		exit(EXIT_FAILURE);
	}
	
	unsigned fc = mbr.mbr_vols[vol].vd_first_cylinder;
	unsigned fs = mbr.mbr_vols[vol].vd_first_sector;
	return (fc+(fs+n_bloc)/HDA_MAXSECTOR);
}

unsigned sector_of_bloc(int vol, int n_bloc){
	if (vol<0 || vol > mbr.mbr_nvol){
		fprintf(stderr, "Wrong volume number\n");
		exit(EXIT_FAILURE);
	}
	if (n_bloc<0 || vol > mbr.mbr_vols[vol].vd_n_sector){
		fprintf(stderr, "Wrong bloc number\n");
		exit(EXIT_FAILURE);
	}
	unsigned fs = mbr.mbr_vols[vol].vd_first_sector;
	return ((fs+n_bloc)%HDA_MAXSECTOR);	
}

void read_bloc(int vol, int n_bloc, unsigned char* buffer){
	unsigned int cyl = cylinder_of_bloc(vol, n_bloc);
	unsigned int sec = sector_of_bloc(vol, n_bloc);
	read_sector(cyl, sec, buffer);
}

void write_bloc(unsigned int vol, unsigned int n_bloc, const unsigned char *buffer){
	unsigned int cyl = cylinder_of_bloc(vol, n_bloc);
	unsigned int sec = sector_of_bloc(vol, n_bloc);
	write_sector(cyl, sec, buffer);
}

void format_vol(unsigned int vol){
	int n_bloc = mbr.mbr_vols[vol].vd_n_sector;
	unsigned int cyl = mbr.mbr_vols[vol].vd_first_cylinder;
	unsigned int sec = mbr.mbr_vols[vol].vd_first_sector;
	format_sector(cyl, sec, n_bloc, 0);
}

char* printType(enum vd_type_e type){
	switch (type){
		case VT_BASE: 	return "VT_BASE";
		case VT_ANX:	return "VT_ANX";
		case VT_OTH:	return "VT_OTH";
	}
}

void printf_vol(unsigned int i){
	printf("Volume %d:\n",i);
		printf("\tsize: %d\n",mbr.mbr_vols[i].vd_n_sector);
		printf("\t(fc, fs):(%d,%d)\n",mbr.mbr_vols[i].vd_first_cylinder,mbr.mbr_vols[i].vd_first_sector);
		printf("\ttype: %s\n", printType(mbr.mbr_vols[i].vd_val_type));
		printf("\t(lc,ls); (%d,%d)\n\n",cylinder_of_bloc(i, mbr.mbr_vols[i].vd_n_sector), sector_of_bloc(i,mbr.mbr_vols[i].vd_n_sector-1));
}

void listVolumes(){
	int i;
	for ( i = 0 ; i < mbr.mbr_nvol; i++){
		printf_vol(i);
	}
}


/* exit if the size is wrong */
void check_sector_size(){
	int real_value;
	_out(HDA_CMDREG, CMD_DSKINFO);
	real_value = (_in(HDA_DATAREGS+4)<<8)|_in(HDA_DATAREGS+5);
	if(real_value != HDA_SECTORSIZE){
		fprintf(stderr, "Error in sectors size\n\tsize found: %d\n\tsize expected: %d\n", real_value,HDA_SECTORSIZE);
		exit(EXIT_FAILURE);
	}
}

void init(){
	int i;
	if(init_hardware("etc/hardware.ini") == 0) {
		fprintf(stderr, "Error in hardware initialization\n");
		exit(EXIT_FAILURE);
	}

	check_sector_size();
	
	for(i=0; i<16; i++) IRQVECTOR[i] = empty_it;
	_mask(1);
	
	load_mbr();
}