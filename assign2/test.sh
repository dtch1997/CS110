gdb ./diskimageaccess
break pathname.c:pathname_lookup_helper
break pathname.c:49
run -p  samples/testdisks/basicDiskImage
