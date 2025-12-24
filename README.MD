# Тестирование

```bash
./sed_simplified ./tests/input1.txt 's/old/NEW/'
./sed_simplified ./tests/input2.txt 's/[0-9]+/X/'
./sed_simplified ./tests/input3.txt '/^DEBUG/d'
./sed_simplified ./tests/input4.txt '/apple/d'
./sed_simplified ./tests/input5.txt 's/^/[PFX] /'
./sed_simplified ./tests/input6.txt 's/$/ [SFX]/'

./sed_simplified ./tests/input7.txt '/^#/d'
./sed_simplified ./tests/input7.txt '/^DEBUG/d'
./sed_simplified ./tests/input7.txt 's/[0-9]+/***/'
```