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

cfdisk "$tmp_file"

echo
fdisk -l "$tmp_file" | grep FAT16 | while IFS=' ' read -r device start end sectors size id fstype
do
	# avoid losetup / mount with sudo
	echo "Processing volume at sector ${start}..."
	volume=$(mktemp)
	dd if="$tmp_file" of="$volume" bs=512 skip="$start" count="$sectors" 2> /dev/null
	mkdosfs "$volume" > /dev/null
	dd if="$volume" of="$tmp_file" bs=512 seek="$start" count="$sectors" conv=notrunc 2> /dev/null
	echo
	rm "$volume"
done 

#exit

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
