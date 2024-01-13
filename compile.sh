#!/bin/bash

while true; do
    # Compile the C program
    gcc cewmain.c cJSON.c report_email_sender.c -o testr -lcurl -lm

    # Run the compiled program
    ./testr

    # Sleep for 60 seconds before running again
    sleep 12
done
#!/bin/bash
