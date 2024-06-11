#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "htables.h"



char *query_words[] = {"example", "words"};
int query_word_count = sizeof(query_words) / sizeof(query_words[0]);


typedef struct {
    int count;              // Counter for each n-gram
    htable_t *continuations;  // Pointer to the secondary hash table
} ngram_entry_t;


#define NGRAM_SIZE 2  // Define the size of n-grams

htable_t global_ngram_table; // Global hash table for storing n-grams

int unpack(const char *path);
char* load_file(const char *path);

void add_ngram(char *ngram, char *suite); //add n-grams to Global-hash 
void scan_ngrams(char *content); //find the ngrams
//printing the goods 
int print_continuation_entry(char *key, void *value);
int print_ngram_entry(char *key, void *value);
//weight_entry 
int weight_entry(char *ngram, void *value);
char best_ngram[256] = {0};
int best_ngram_score = 0;
//text generation 
void gentext(int count);







int main() {
    ht_init(&global_ngram_table, free);  // Initialize the global hash table

    char path[] = "txt.tar.bz2";
    if (unpack(path)) {  // Only proceed if unpacking is successful
        DIR *dir; 
        struct dirent *entry;
        dir = opendir("./txt");

        if (dir == NULL) {
            perror("Unable to open directory");
            return 1;  // Exit if directory cannot be opened
        }

        while ( (entry = readdir(dir)) != NULL) {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".txt") == 0) {
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "./txt/%s", entry->d_name);
                char *content = load_file(filepath);
                if (content) {
                    scan_ngrams(content);  // Process the content for n-grams
                    free(content);
                }
            }
            
        }

        closedir(dir);
    }


    hsize_t ngram_count = ht_len(global_ngram_table);
    printf("Le nombre total de n-gramme trouver est : %lu\n", (unsigned long)ngram_count);
    ht_visit(global_ngram_table, weight_entry);  // Trouver le meilleur n-gramme


    // Générer le text
    gentext(50);
    //ht_visit(global_ngram_table, print_ngram_entry);


    // Clean up the hash table
     ht_clear(&global_ngram_table);
    ht_free(&global_ngram_table);

    return 0;
    }


void scan_ngrams(char *content) {
    char *tokens[NGRAM_SIZE + 1]; // Array to hold tokens for n-grams and the next token
    int index = 0;
    char *token = strtok(content, " \n\t\r");  // Tokenize by whitespace

    while (token != NULL) {
        if (index < NGRAM_SIZE + 1) {
            tokens[index++] = token;
        }

        if (index == NGRAM_SIZE + 1) {
            // Form the n-gram and the suite
            char ngram[256] = "";
            for (int i = 0; i < NGRAM_SIZE; i++) {
                strcat(ngram, tokens[i]);
                if (i < NGRAM_SIZE - 1) strcat(ngram, " ");
            }
            add_ngram(ngram, tokens[NGRAM_SIZE]);
            
            // Shift tokens to the left to process the next n-gram in sequence
            for (int i = 1; i <= NGRAM_SIZE; i++) {
                tokens[i-1] = tokens[i];
            }
            index--;
        }
        token = strtok(NULL, " \n\t\r");
    }
}

void add_ngram(char *ngram, char *suite) {
    ngram_entry_t *entry;
    void *value;

    // Check if the n-gram already exists in the global hash table
    if (ht_get(global_ngram_table, ngram, &value)) {
        entry = (ngram_entry_t *)value;
    } else {
        // If not, create a new entry
        entry = malloc(sizeof(ngram_entry_t));
        if (!entry) {
            perror("Failed to allocate memory for ngram_entry");
            exit(EXIT_FAILURE);
        }
        entry->count = 0; // Initialize the counter
        entry->continuations = malloc(sizeof(htable_t));
        if (!entry->continuations) {
            perror("Failed to allocate memory for continuations hash table");
            exit(EXIT_FAILURE);
        }
        ht_init(entry->continuations, free); // Initialize the secondary hash table
        ht_set(&global_ngram_table, strdup(ngram), entry, NULL);
    }

    // Increment the n-gram count
    entry->count++;

    // Manage the secondary hash table for continuations
    unsigned int *continuation_count;
    if (ht_get(*entry->continuations, suite, &value)) {
        continuation_count = (unsigned int *)value;
        (*continuation_count)++;
    } else {
        continuation_count = malloc(sizeof(unsigned int));
        *continuation_count = 1; // Initialize the count
        ht_set(entry->continuations, strdup(suite), continuation_count, NULL);
    }
}



int unpack(const char *path) {
    struct stat st;
    
    if (stat("txt", &st) == 0) {
        printf("Le répertoire 'txt' existe déjà.\n");
        return 1; 
    }

    char command[256];
    snprintf(command, sizeof(command), "tar xjf %s", path);
    int result = system(command);

    if (result == 0) {
        printf("La décompression s'est terminée avec succès.\n");
        return 1;
    } else {
        printf("Erreur lors de la décompression.\n");
        return 0;
    }
}

char* load_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    // Seek to the end to determine the file size
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    rewind(file);

    // Allocate memory for the entire file content plus a null terminator
    char *string = malloc(fsize + 1);
    if (!string) {
        fclose(file);
        fprintf(stderr, "Failed to allocate memory for file contents.\n");
        return NULL;
    }

    // Read the file into the allocated memory and null-terminate it
    fread(string, 1, fsize, file);
    string[fsize] = '\0';

    // Close the file and return the content
    fclose(file);
    return string;
}

int print_ngram_entry(char *key, void *value) {
    ngram_entry_t *entry = (ngram_entry_t *)value;
    printf("N-gram: '%s' has %d instances and the following continuations:\n", key, entry->count);

    // Iterate through the secondary hash table to print continuations
    ht_visit(*entry->continuations, print_continuation_entry);
    return 1;  // Continue traversal
}

int print_continuation_entry(char *key, void *value) {
    unsigned int count = *(unsigned int *)value;
    printf("\tContinuation '%s' -> %d times\n", key, count);
    return 1;  // Continue traversal
}
/*______________________________________________________________________________________________________________*/

int weight_entry(char *ngram, void *value) {
    ngram_entry_t *entry = (ngram_entry_t *)value;
    int score = 0;

    // Check if each query word is in the n-gram
    for (int i = 0; i < query_word_count; i++) {
        char *found = strstr(ngram, query_words[i]);
        if (found) {  // If the query word is found within the n-gram
            score += strlen(query_words[i]); // Add the length of the word to score
        }
    }

    // Multiply the sum of lengths by the count of the n-gram
    score *= entry->count;

    // Check if this score is better than the current best
    if (score > best_ngram_score) {
        strncpy(best_ngram, ngram, sizeof(best_ngram));
        best_ngram_score = score;
    }

    return 1; // Continue visiting other entries
}





void gentext(int count) {
    char currentNgram[256];
    if (strlen(best_ngram) > 0) {
        strcpy(currentNgram, best_ngram);  // Commencer par le meilleur n-gramme
        printf("Generated text starts with: \"%s\"", currentNgram);
    } else {
        printf("No starting n-gram found.\n");
        return;
    }

    srand(time(NULL));  // Initialiser le générateur de nombres aléatoires pour la sélection des suites
    char nextWord[256];
    int wordsGenerated = NGRAM_SIZE;  // Commencer le compte par la taille du n-gramme

    while (wordsGenerated < count) {
        ngram_entry_t *entry;
        void *value;

        if (!ht_get(global_ngram_table, currentNgram, &value)) {
            printf(".\n");  // Fin de la génération si aucune suite n'est trouvée
            break;
        }

        entry = (ngram_entry_t *)value;
        int totalContinuations = entry->count;  // Le nombre total de suites possibles
        int randomPick = rand() % totalContinuations;  // Choisir une suite au hasard
        int cumulative = 0;

        for (hsize_t j = 0; j < entry->continuations->used; j++) {
            unsigned int *continuationCount = (unsigned int *)entry->continuations->slots[j].val;
            cumulative += *continuationCount;
            if (cumulative > randomPick) {
                strcpy(nextWord, entry->continuations->slots[j].key);
                printf(" %s", nextWord);
                // Préparer le n-gramme suivant
                snprintf(currentNgram, sizeof(currentNgram), "%s %s", strchr(currentNgram, ' ') ? strchr(currentNgram, ' ') + 1 : "", nextWord);
                wordsGenerated++;
                break;
            }
        }
    }

    printf("\n");
}
