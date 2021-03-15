# Exam Rank 04

>Test filedes leaks with ``lsof -c microshell``
>
## How to use
The file you want to test is microshell.c with the main function.
To test, apply the following modifications to this file.
1. Add the code below to the top of the miroshell.c
```
# ifdef TEST_SH
# define TEST 1
# else
# define TEST 0
#endif
```
2. Add the code below immediately before the main function returns
>while(TEST);
3. Run the test with the following command
>./test.sh
## Ressources

* [lsof examples](https://www.thegeekstuff.com/2012/08/lsof-command-examples/)
