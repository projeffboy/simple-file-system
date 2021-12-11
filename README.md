For tests 0-2:
```bash
make clean && make && ./jefftang_sfs && make clean
```

For testing `sfs_newfile` and `sfs_oldfile`:
```bash
chmod 744 ./sfs_newfile
chmod 744 ./sfs_oldfile

mkdir mytemp
./sfs_newfile mytemp
cp Makefile README.md mytemp # add some files into mytemp
strings file.sfs # should see text
killall sfs_newfile # or `fusermount -u mytemp`
# mytemp should be empty
./sfs_oldfile mytemp # mytemp should not be empty
killall old_newfile # or `fusermount -u mytemp`

rm -r mytemp file.sfs
```

For the `fuse_wrap_new.c` and `fuse_wrap_old.c` tests:
```bash
mkdir mytemp

make clean && make && ./jefftang_sfs mytemp # fuse_wrap_new.c
cp Makefile README.md mytemp # add some files into mytemp
strings fs.sfs # should see text
killall jefftang_sfs # or `fusermount -u mytemp`
# mytemp should be empty
make clean && make && ./jefftang_sfs mytemp # fuse_wrap_old.c
./jefftang_sfs mytemp # mytemp should not be empty
killall old_newfile # or `fusermount -u mytemp`

rm -r mytemp fs.sfs
```

test 1 and 2: 1047 iterations, 1072128 bytes

prof test: 268 iterations, 274432 bytes