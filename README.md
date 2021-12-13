I got tests 0-2 working with 0 errors, although once every like 40 tries I get a minor error (e.g. ERROR: Requested 115 bytes, read 114). Fuse wrapper tests work fine based on my quick demo. If you get different results than me you can let me know.

For tests 0-2:
```bash
make clean && make && ./jefftang_sfs && make clean
```

For the `fuse_wrap_new.c` and `fuse_wrap_old.c` tests:
```bash
mkdir mytemp

# fuse_wrap_new.c
make clean && make && ./jefftang_sfs mytemp
strings fs.sfs # should be empty
cp Makefile README.md mytemp # add some files into mytemp
strings fs.sfs # should see text
ls mytemp # not empty
killall jefftang_sfs # or `fusermount -u mytemp`
ls mytemp # empty

# fuse_wrap_old.c
make clean && make && ./jefftang_sfs mytemp
ls mytemp # not empty
rm mytemp/*
ls mytemp # empty
echo aaron > mytemp/names.txt
strings fs.sfs # you should still see the deleted file contents, because those bytes were not overwritten. You may even see the names of the deleted files, but remember their mode is set to 0, which is why they don't show up. When I ran this, filename `/Makefile` doesn't even show up, because it got overwritten by `/names.txt`.
ls mytemp # not empty
cat mytemp/Makefile # nothing
cat mytemp/names.txt # something

killall jefftang_sfs # or `fusermount -u mytemp`
ls mytemp # empty
```


For testing `sfs_newfile` and `sfs_oldfile` (don't even think this is necessary):
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

my test 1 and 2: 267 iterations, 273408 bytes

prof test: 268 iterations, 274432 bytes