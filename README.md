I got tests 0-2 and `fuse_wrap_new.c` working with no errors. I can't get `fuse_wrap_old.c` to work. If you get different results than me you can let me know.

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
ls mytemp # not empty
killall sfs_newfile # or `fusermount -u mytemp`
ls mytemp # empty

./sfs_oldfile mytemp
ls mytemp # not empty
killall sfs_oldfile # or `fusermount -u mytemp`
ls mytemp # empty

rm -r mytemp file.sfs
```

For the `fuse_wrap_new.c` and `fuse_wrap_old.c` tests:
```bash
mkdir mytemp

make clean && make && ./jefftang_sfs mytemp # fuse_wrap_new.c
cp Makefile README.md mytemp # add some files into mytemp
strings fs.sfs # should see text
ls mytemp # not empty
killall jefftang_sfs # or `fusermount -u mytemp`
ls mytemp # empty

make clean && make && ./jefftang_sfs mytemp # fuse_wrap_old.c
killall jefftang_sfs # or `fusermount -u mytemp`
```

my test 1 and 2: 267 iterations, 273408 bytes

prof test: 268 iterations, 274432 bytes