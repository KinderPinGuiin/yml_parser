#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "yaml_parser.h"

int main(void) {
  yml_parser *parser = init_yml_parser("test.yml", NULL);
  exec_parser(parser);
  char server_name[255];
  get(parser, "name", server_name);
  fprintf(stderr, "Nom du serveur : %s\n", server_name);
  int nb_slots;
  get(parser, "slots", &nb_slots);
  fprintf(stderr, "Nombre de slots : %d\n", nb_slots);
  free_parser(parser);

  return EXIT_SUCCESS;
}