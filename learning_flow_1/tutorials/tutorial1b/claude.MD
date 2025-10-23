A Small Database of Servers

This exercise is intended to be challenging you. Somebody else has written some code. It uses structs, and typedefs, and pointers, and addresses, and arrays, and all that. This person, however, did not quite complete the job: there are bits missing, and the compiler may throw an error here or there.

Your job, should you chose to accept it, is to fix the problems, complete the program, and basically render the code sound and safe.
Step 1: Grab a copy of the code

The files are idserver.c idserver.h and pointer.c. Oh and also the Makefile. You can download the zip file from this link (also available at the folder Template files (C) of this Tutorial).

Then make and run the program pointer. Make sure that you understand what the output actually means. If there are warning messages given, you can ignore them for the moment. Of course, at the end of this sheet, you should have no error messages.

Then, read the code. Pay special attention to the address-of (&), deference (*), arrow (->), dot (.) operators.

If you're in the mood for it (and, you really should be) take a look at the Makefile. It contains a couple of nice tricks that you may want to use (then again, you will do fine without it, also).

Once you feel that you have a good grip on what the program does, move on to the next step.
Step 2: Print information of a server

Remove the commented-out parts labeled "step 2" in pointer.c. Complete the print_idserver function so that the program yields the following output:

--results of print_idserver--

Id: cmok
Latency (usec): 600
Region: eur
Status: up
Nbr of threads: 70

Step 3: Modify a record

Remove the commented-out parts labeled "step 3" in pointer.c.  Complete the function:

void modify(idserver s, char *id, int latency, char *status)

This function is supposed to modify the idserver s to the id, latency and status provided (note that char *status and char[] status are equivalent when used as function arguments).

Also complete the function

void modify_by_pointer(idserver *s, char *id, int latency, char *status);

This function is supposed to do the same thing as modify.

Will they both work? And why? Or why not?
Step 4: Pointers and implicit memory copy

Remove the commented-out parts labeled "step 4" in pointer.c. Run the program and explain the output.
Step 5: Create a new server

Remove the commented-out parts labeled "step 5" in pointer.c. This piece of code tries to create a new server by using create_idserver function.

Run the program and ... huh ... what the heck does the output mean?

If the program doesn't work, modify the create_idserver function to give the correct output (you should not modify the signature, but only the content).

A tip: try to find out the difference between "stack" and "heap" in C programming.

After getting the correct output, are there still potential issues with this code? Is it robust?
To Have Done Before Proceeding

    Implement all the specified functionalities
    Have the program compiling (without errors)
