/**
 * File: pipeline-test.c
 * ---------------------
 * Exercises the pipeline function to verify 
 * basic functionality.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

static void printArgumentVector(char *argv[]) {
  if (argv == NULL || *argv == NULL) {
    printf("<empty>");
    return;
  }
  
  while (true) {
    printf("%s", *argv);
    argv++;
    if (*argv == NULL) return;
    printf(" ");
  }
}

static void summarizePipeline(char *argv1[], char *argv2[]) {
  printf("Pipeline: ");
  printArgumentVector(argv1);
  printf(" -> ");
  printArgumentVector(argv2);
  printf("\n");
}

static void launchPipedExecutables(char *argv1[], char *argv2[]) {
  summarizePipeline(argv1, argv2);
  pid_t pids[2];
  pipeline(argv1, argv2, pids);
  waitpid(pids[0], NULL, 0);
  waitpid(pids[1], NULL, 0);
}

static void simpleTest() {
  char *argv1[] = {"cat", "/usr/include/tar.h", NULL};
  char *argv2[] = {"wc", NULL};
  launchPipedExecutables(argv1, argv2);
}


static void timedTest() {
  time_t start = time(NULL);
  char *argv[] = {"sleep", "2", NULL};
  launchPipedExecutables(argv, argv);
  time_t end = time(NULL);
  printf("Program took %d seconds\n", (int)( end-start));
}

static void xargsTest() {
  char* argv1[] = {"echo", "Hello World", NULL};
  char *argv2[] = {"xargs", "echo", NULL};
  launchPipedExecutables(argv1, argv2);
}

static void veryLongOutputTest() {
  char *argv1[] = {"ls", "-aR", "/usr", NULL};
  char *argv2[] = {"wc", NULL};
  launchPipedExecutables(argv1, argv2);
}

static void emptyOutputTest() {
  char *argv1[] = {"echo", NULL};
  char *argv2[] = {"wc", NULL};
  launchPipedExecutables(argv1, argv2);
}

int main(int argc, char *argv[]) {
  simpleTest();
  timedTest();
  xargsTest();
  veryLongOutputTest();
  emptyOutputTest();
  return 0;
}
