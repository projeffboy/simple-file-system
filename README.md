I got tests 0-2 and `fuse_wrap_new.c` working with no errors. I can't get `fuse_wrap_old.c` to work. If you get something different you can let me know.

For tests 0-2:
```bash
make && ./jefftang_sfs && make clean
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
killall sfs_oldfile # or `fusermount -u mytemp`

rm -r mytemp file.sfs
```

For the `fuse_wrap_new.c` and `fuse_wrap_old.c` tests:
```bash
mkdir mytemp

make && ./jefftang_sfs mytemp # fuse_wrap_new.c
cp Makefile README.md mytemp # add some files into mytemp
strings fs.sfs # should see text
killall jefftang_sfs # or `fusermount -u mytemp`
# mytemp should be empty
make && ./jefftang_sfs mytemp # fuse_wrap_old.c
killall jefftang_sfs # or `fusermount -u mytemp`
```

my test 1 and 2: 267 iterations, 273408 bytes

prof test: 268 iterations, 274432 bytes