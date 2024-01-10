#include "email_sender.h"
#include <stdio.h>


int main() {
    const char *to = "khalid4506093@cloud.neduet.edu.pk";
    const char *file_path = "average.txt";
    

    int result = send_email_with_attachment(to, file_path);

    if (result == 0) {
        printf("Email sent successfully.\n");
    } else {
        printf("Failed to send email.\n");
    }

    return 0;
}
