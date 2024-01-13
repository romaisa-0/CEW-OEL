#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "email_sender.h"
#include <string.h>
#include <time.h>

struct HourlyData {
    char time[20];
    double temperature;
    double humidity;
};

struct DailyData {
    char date[20];
    double averageTemperature;
    double averageHumidity;
    int count;
    struct HourlyData hourlyData[24];
};

size_t WriteCallback(void *contents, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(contents, size, nmemb, stream);
}

int compareDates(const char *date1, const char *date2) {
    return strncmp(date1, date2, 10);
}

void printTodayAverage(struct DailyData *dailyData, int dayIndex, const char *currentDate) {
    if (dayIndex < 0) {
        return;
    }

    if (compareDates(dailyData[dayIndex].date, currentDate) == 0) {
        printf("\nToday's Average:\n");
        dailyData[dayIndex].averageTemperature /= dailyData[dayIndex].count;
        dailyData[dayIndex].averageHumidity /= dailyData[dayIndex].count;
        printf("Date: %s\n Average Temperature: %.1f°C\n Average Humidity: %.2f\n\n",
               dailyData[dayIndex].date, dailyData[dayIndex].averageTemperature, dailyData[dayIndex].averageHumidity);

        FILE *current = fopen("current.txt", "w");
        if (!current) {
            fprintf(stderr, "Error opening file for writing.\n");
            return;
        }
        fprintf(current, "\n");
        fprintf(current, "Date: %s\n Average Temperature: %.1f°C\n Average Humidity: %.2f\n\n",
                dailyData[dayIndex].date, dailyData[dayIndex].averageTemperature, dailyData[dayIndex].averageHumidity);

        if (dailyData[dayIndex].averageTemperature > 20) {
            printf("Weather Update: It is hot today!\n");
            fprintf(current, "Weather Update: It is hot today!\n");
            printf("Tips: Stay hydrated, wear light clothing, and avoid direct sunlight.\n");
            fprintf(current, "Tips: Stay hydrated, wear light clothing, and avoid direct sunlight.\n");
        } else {
            printf("Weather Update: It is cold today!\n");
            fprintf(current, "Weather Update: It is cold today!\n");
            printf("Tips: Dress in layers, wear warm clothing, and keep yourself warm.\n");
            fprintf(current, "Tips: Dress in layers, wear warm clothing, and keep yourself warm.\n");
        }
        fclose(current);
    }
}

void printDailyAverages(struct DailyData *dailyData, int dayIndex) {
    if (dayIndex < 0) {
        return;
    }

    for (int i = 0; i <= dayIndex; i++) {
        if (dailyData[i].count > 0) {
            dailyData[i].averageTemperature /= dailyData[i].count;
            dailyData[i].averageHumidity /= dailyData[i].count;

            FILE *average = fopen("average.txt", "a");
            if (!average) {
                fprintf(stderr, "Error opening file for writing.\n");
                return;
            }
            fprintf(average, "\n");
            fprintf(average, "Date: %s\n Average Temperature: %.1f°C\n Average Humidity: %.2f\n\n",
                    dailyData[i].date, dailyData[i].averageTemperature, dailyData[i].averageHumidity);
            fclose(average);
        }
    }
}

int main() {
    CURL *hnd = curl_easy_init();

    if (!hnd) {
        fprintf(stderr, "Failed to initialize curl.\n");
        return 1;
    }

    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(hnd, CURLOPT_URL, "http://api.weatherapi.com/v1/forecast.json?key=9f8be5c7fde74f67b4892632240101&q=London&days=5&aqi=no&alerts=no");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "X-RapidAPI-Key: 9f8be5c7fde74f67b4892632240101");
    headers = curl_slist_append(headers, "X-RapidAPI-Host: api.weatherapi.com");
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);

    FILE *outputFile = fopen("response.txt", "w");
    if (!outputFile) {
        fprintf(stderr, "Error opening file for writing.\n");
        return 1;
    }

    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, outputFile);

    CURLcode ret = curl_easy_perform(hnd);

    fclose(outputFile);

    curl_easy_cleanup(hnd);
    curl_slist_free_all(headers);

    if (ret != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));
        return 1;
    }

    FILE *inputFile = fopen("response.txt", "r");
    if (!inputFile) {
        fprintf(stderr, "Error opening file for reading.\n");
        return 1;
    }

    fseek(inputFile, 0, SEEK_END);
    long fileSize = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    char *jsonData = malloc(fileSize + 1);
    if (!jsonData) {
        fprintf(stderr, "Memory allocation error.\n");
        fclose(inputFile);
        return 1;
    }

    fread(jsonData, 1, fileSize, inputFile);
    fclose(inputFile);

    jsonData[fileSize] = '\0';

    cJSON *json = cJSON_Parse(jsonData);
    free(jsonData);

    if (!json) {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return 1;
    }

    cJSON *forecast = cJSON_GetObjectItem(json, "forecast");
    if (!forecast) {
        fprintf(stderr, "Error accessing forecast object.\n");
        cJSON_Delete(json);
        return 1;
    }

    cJSON *forecastday = cJSON_GetObjectItem(forecast, "forecastday");
    if (!cJSON_IsArray(forecastday)) {
        fprintf(stderr, "Error accessing forecastday array.\n");
        cJSON_Delete(json);
        return 1;
    }

    struct DailyData dailyData[10];
    int lastDayIndex = -1;

    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);
    char currentDate[20];
    strftime(currentDate, sizeof(currentDate), "%Y-%m-%d", tm_info);

    for (int i = 0; i < cJSON_GetArraySize(forecastday); i++) {
        cJSON *day = cJSON_GetArrayItem(forecastday, i);
        cJSON *dateItem = cJSON_GetObjectItem(day, "date");
        cJSON *hourArray = cJSON_GetObjectItem(day, "hour");

        if (cJSON_IsString(dateItem) && cJSON_IsArray(hourArray)) {
            const char *date = dateItem->valuestring;

            if (lastDayIndex == -1 || compareDates(date, dailyData[lastDayIndex].date) != 0) {
                lastDayIndex++;
                strcpy(dailyData[lastDayIndex].date, date);
                dailyData[lastDayIndex].averageTemperature = 0.0;
                dailyData[lastDayIndex].averageHumidity = 0.0;
                dailyData[lastDayIndex].count = 0;
            }

            for (int j = 0; j < cJSON_GetArraySize(hourArray); j++) {
                cJSON *hour = cJSON_GetArrayItem(hourArray, j);
                cJSON *timeItem = cJSON_GetObjectItem(hour, "time");
                cJSON *tempItem = cJSON_GetObjectItem(hour, "temp_c");
                cJSON *humItem = cJSON_GetObjectItem(hour, "humidity");

                if (cJSON_IsString(timeItem) && cJSON_IsNumber(tempItem)) {
                    const char *time = timeItem->valuestring;
                    double temperature = tempItem->valuedouble;
                    double humidity = humItem->valuedouble;

                    strcpy(dailyData[lastDayIndex].hourlyData[j].time, time);
                    dailyData[lastDayIndex].hourlyData[j].temperature = temperature;
                    dailyData[lastDayIndex].hourlyData[j].humidity = humidity;

                    dailyData[lastDayIndex].averageTemperature += temperature;
                    dailyData[lastDayIndex].averageHumidity += humidity;
                    dailyData[lastDayIndex].count++;
                }
            }

            printTodayAverage(dailyData, lastDayIndex, currentDate);
        }
    }

    printDailyAverages(dailyData, lastDayIndex);

    cJSON_Delete(json);

    const char *to = "khalid4506093@cloud.neduet.edu.pk";
    const char *file_path = "current.txt";

    int result = send_email_with_attachment(to, file_path);

    if (result == 0) {
        printf("Email sent successfully.\n");
    } else {
        printf("Failed to send email.\n");
    }

    return 0;
}

