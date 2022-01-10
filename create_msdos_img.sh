#!/bin/sh -eu

if [ $# -ne 3 ]
then
	echo "Usage: ${0} <start sector> <end sector> <disk image>"
	echo
	echo "<start sector> and <end sector> represent start/end sector"
	echo "of given Atari partition (taken from e.g. HD Driver)"
	exit 1
fi

start_sector=${1}
end_sector=${2}
count=$(($end_sector-$start_sector+1-1))
disk_image=${3}

tmp_file=$(mktemp)
dd if="$disk_image" of="$tmp_file" bs=512 skip=$(($start_sector+1)) count="$count" 2> /dev/null

echo "Starting cfdisk..."
cfdisk "$tmp_file"

fdisk -c=dos -b 512 -t dos -o Start,Sectors,Type -l "$tmp_file" | grep FAT16 | while IFS=' ' read -r start sectors fstype
do
    echo "Processing MS-DOS volume at sector ${start} of size ${sectors} sectors..."
    echo
	echo "Mount as: sudo mount -o loop,offset=$((($start_sector+1+$start)*512)) \"$disk_image\" /mnt"

	# DOS FORMAT expects DOS FDISK to clear the first 512 bytes (man fdisk) but let's clear the whole volume to have a fresh start
	dd if=/dev/zero of="$tmp_file" bs=512 seek="$start" count="$sectors" conv=notrunc 2> /dev/null
	# block count is specified in KiB (1024)
	# TODO: is -h always $start and not $start+previous_partition_size? is sectors per track == $start?
	mkdosfs -a -g 64/34 -h "$start" --offset="$start" "$tmp_file" $(($sectors*512/1024)) > /dev/null
	# WARNING: hardcoded offset (assumes 0xeb 0x3c 0x90 at $start)
	dd if=bootloader.bin of="$tmp_file" bs=1 seek=$(($start*512+62)) conv=notrunc 2> /dev/null
done 

echo
read -p "Write changes back to $disk_image? (y/n) " yn
case $yn in
	[Yy]* )
	  # disable Atari root sector
	  #dd if=/dev/zero   of="$disk_image" bs=512 seek=$(($start_sector+0)) count=1        conv=notrunc 2> /dev/null
	  dd if="$tmp_file" of="$disk_image" bs=512 seek=$(($start_sector+1)) count="$count" conv=notrunc 2> /dev/null
	  ;;
esac

rm "$tmp_file"

echo "Done."
