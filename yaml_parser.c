#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>
#include <string.h>
#include <semaphore.h>
#include "yml_parser.h"

/*
 * Macros-fonctions
 */

#define FILL_ERROR(val) if (error != NULL) {                                  \
                          *error = val;                                       \
                        }

/*
 * Macros
 */

#define YML_STRING_REGEX "([a-z0-9_]+)\\s*:\\s*\"([a-z0-9_ ]+)\"(\r?\n)*"
#define YML_INT_REGEX "([a-z0-9_]+)\\s*:\\s*(-?[0-9]+)(\r?\n)*"

#define KEY_GROUP 1
#define VALUE_GROUP 2

#define YML_INT_TYPE 1
#define YML_STRING_TYPE 2

#define NOT_EXECUTED 0
#define EXECUTED 1

/*
 * Structures de données
 */

typedef struct yml_hash_map yml_hash_map;
typedef struct yml_hash_map_elem yml_hash_map_elem;

/**
 * Structure du parseur.
 */
struct yml_parser {
  sem_t mutex;
  char *content;
  yml_hash_map *map;
  int executed;
};

/**
 * Table de hashage contenant les pairs clés / valeurs du fichier.
 */
struct yml_hash_map {
#define MAP_SIZE 256
  yml_hash_map_elem *elems[MAP_SIZE];
};

/**
 * Cellule de liste de valeur.
 */
struct yml_hash_map_elem {
  void *value;
  char *key;
  size_t size;
  yml_hash_map_elem *next;
};

/*
 * Fonctions
 */

/**
 * Applique la regex reg sur str et exécute la fonction func prenant en 
 * paramètre : context, les résultats du match courant de la regex, et 
 * l'accumulateur. La fonction renverra le nouvel accumulateur.
 * 
 * @param {const char *} La chaîne où appliquer la regex.
 * @param {const char *} La regex.
 * @param {int (*)(void *, regmatch_t *, int)} La fonction à appliquer.
 * @param {void *} Le contexte de la fonction.
 * @return {int} 1 si tout se passe bien et une valeur négative sinon.
 */
static int reg_apply_func(const char *str, const char *reg, 
    int (*func)(void *, const char *, regmatch_t *, int), void *context,
    int acc);

/**
 * Extrait la clé et les valeurs dans str grâce à matches, les stock dans map 
 * et renvoie le nouvel accumulateur.
 * 
 * @param {yml_hash_map *} La table de hachage où stocker les valeurs.
 * @param {const char *} La chaîne où extraire les clés valeurs.
 * @param {regmatch_t *} Les match d'une regex.
 * @param {int} L'accumulateur.
 * @return {int} Le nouvel accumulateur.
 */
static int insert_keys_values(yml_hash_map *map, const char *str, 
    regmatch_t *matches, int acc);

/**
 * Insère dans hash_map l'élement de clé key et de valeur value. Retourne 1 en,
 * cas de succès, 0 si l'élement a été remplacé car celui-ci était déjà 
 * présent, ou un nombre négatif en cas d'échec.
 * 
 * @param {yml_hash_map *} La table de hachage.
 * @param {const char *} La clé de l'élement.
 * @param {void *} La valeur de l'élement.
 * @param {size_t} La taille de l'élement.
 * @return {int} 1 en cas de succès, 0 si la valeur a été remplacée, un nombre 
 *               négatif en cas d'échec.
 */
static int hashmap_insert(yml_hash_map *hash_map, const char *key, 
    void *value, size_t elem_size);

/**
 * Libère la table de hachage associée à map.
 * 
 * @param {yml_hash_map *} La table de hachage.
 */
static void free_hash_map(yml_hash_map *map);

/**
 * L'une des fonctions de pré-hachage consillées par Kernighan et Pike pour 
 * les chaines de caractères.
 */
static size_t str_hashfun(const char *s);

yml_parser *init_yml_parser(const char *path, int *error) {
  if (path == NULL) {
    FILL_ERROR(YML_INVALID_POINTER)
    return NULL;
  }
  // Initialise le parseur
  yml_parser *parser = malloc(sizeof(*parser));
  if (parser == NULL) {
    FILL_ERROR(YML_NOT_ENOUGH_MEMORY);
    return NULL;
  }
  if (sem_init(&parser->mutex, 1, 1) < 0) {
    FILL_ERROR(YML_SEM_ERROR);
    free(parser);
    return NULL;
  }
  yml_hash_map *hash_map = malloc(sizeof(*hash_map));
  if (hash_map == NULL) {
    FILL_ERROR(YML_NOT_ENOUGH_MEMORY);
    free(parser);
    return NULL;
  }
  parser->map = hash_map;
  // Ouvre le fichier
  int fd;
  if ((fd = open(path, O_RDONLY)) < 0) {
    FILL_ERROR(YML_INVALID_FILE);
    free(parser);
    return NULL;
  }
  // Récupère sa taille
  ssize_t length = (ssize_t) lseek(fd, 0, SEEK_END);
  if (length < 0) {
    FILL_ERROR(YML_FILE_ERROR);
    free(parser);
    return NULL;
  }
  if (lseek(fd, 0, SEEK_SET) < 0) {
    FILL_ERROR(YML_FILE_ERROR);
    free(parser);
    return NULL;
  }
  // Stock le contenu du fichier dans une chaîne
  char *content = malloc((size_t) length + 1);
  if (content == NULL) {
    FILL_ERROR(YML_NOT_ENOUGH_MEMORY);
    free(parser);
    free(hash_map);
    return NULL;
  }
  if (read(fd, content, (size_t) length) < 0) {
    FILL_ERROR(YML_FILE_ERROR);
    free(parser);
    free(hash_map);
    return NULL;
  }
  content[length] = '\0';
  // Ajoute le contenu dans le parseur
  parser->content = content;
  parser->executed = NOT_EXECUTED;

  return parser;
}

int exec_parser(yml_parser *parser) {
  if (sem_wait(&parser->mutex) < 0) {
    return YML_SEM_ERROR;
  }
  if (parser->executed == EXECUTED) {
    return YML_ALREADY_EXECUTED;
  }
  int r = 0;
  r = reg_apply_func(parser->content, YML_INT_REGEX, 
        (int (*)(void *, const char *, regmatch_t *, int)) insert_keys_values, 
        parser->map, YML_INT_TYPE);
  if (r < 0) {
    return -1;
  }
  r = reg_apply_func(parser->content, YML_STRING_REGEX, 
        (int (*)(void *, const char *, regmatch_t *, int)) insert_keys_values, 
        parser->map, YML_STRING_TYPE);
  if (r < 0) {
    return -1;
  }
  parser->executed = EXECUTED;
  if (sem_post(&parser->mutex) < 0) {
    return YML_SEM_ERROR;
  }

  return 1;
}

int get(yml_parser *parser, const char *key, void *buffer) {
  if (parser == NULL || buffer == NULL) {
    return YML_INVALID_POINTER;
  }
  yml_hash_map_elem *elem = parser->map->elems[str_hashfun(key) % MAP_SIZE];
  while (elem != NULL && strcmp(elem->key, key) != 0) {
    elem = elem->next;
  }
  if (elem == NULL) {
    return 0;
  }
  memcpy(buffer, elem->value, elem->size);

  return 1;
}

int free_parser(yml_parser *parser) {
  if (parser == NULL) {
    return YML_INVALID_POINTER;
  }
  if (sem_wait(&parser->mutex) < 0) {
    return YML_SEM_ERROR;
  }
  free_hash_map(parser->map);
  free(parser->content);
  free(parser);

  return 1;
}

/*
 * Fonctions outils
 */

static int reg_apply_func(const char *str, const char *reg, 
    int (*func)(void *, const char *, regmatch_t *, int), void *context, 
    int acc) {
  regex_t regex;
  // Compile la regex
  int err = regcomp(&regex, reg, REG_EXTENDED | REG_ICASE);
  if (err) {
    return -1;
  }
  // Le nombre maximum de match sera la longueur de la chaîne divisée par 3.
  size_t max_matches = strlen(str) / 3;
  // Le nombre maximum de groupe sera 3 ((clé): (valeur))
  size_t max_group = 4;
  regmatch_t matches[max_group];
  const char *cursor = str;
  // Execute la regex dans la chaîne
  for (size_t i = 0; i < max_matches; ++i) {
    if (regexec(&regex, cursor, max_group, matches, 0)) {
      break;
    }
    acc = func(context, cursor, matches, acc);
    // Déplace le curseur dans la chaîne
    cursor += matches[0].rm_eo;
  }
  regfree(&regex);

  return acc;
}

static int insert_keys_values(yml_hash_map *map, const char *str, 
    regmatch_t *matches, int acc) {
  if (acc < 0) {
    return acc;
  }
  // Extrait la clé
  size_t len = (size_t) (matches[KEY_GROUP].rm_eo - matches[KEY_GROUP].rm_so);
  char key[len + 1];
  strncpy(key, str + matches[KEY_GROUP].rm_so, len);
  key[len] = '\0';
  // Extrait la valeur
  len = (size_t) (matches[VALUE_GROUP].rm_eo - matches[VALUE_GROUP].rm_so);
  char value[len + 1];
  strncpy(value, str + matches[VALUE_GROUP].rm_so, len);
  value[len] = '\0';
  // Convertit la valeur si besoin
  int to_int = 0;
  void *converted_value = NULL;
  size_t size = 0;
  if (acc == YML_INT_TYPE) {
    to_int = atoi(value);
    converted_value = &to_int;
    size = sizeof(int);
  } else {
    converted_value = value;
    size = strlen(value) + 1;
  }
  if (hashmap_insert(map, key, converted_value, size) < 0) {
    acc = -1;
  }

  return acc;
}

static int hashmap_insert(yml_hash_map *hash_map, const char *key, 
    void *value, size_t elem_size) {
  size_t index = str_hashfun(key) % MAP_SIZE;
  yml_hash_map_elem **elem_list = &(hash_map->elems[index]);
  // S'il n'y a pas encore d'élement on l'ajoute en tête
  if (*elem_list != NULL) {
    // Sinon c'est qu'il y a eu collision, dans ces cas-là on ajoute la valeur 
    // en queue de liste
    while ((*elem_list)->next != NULL) {
      elem_list = &(*elem_list)->next;
    }
    (*elem_list)->next = malloc(sizeof(**elem_list));
    if (elem_list == NULL) {
      return -1;
    }
    elem_list = &(*elem_list)->next;
  } else {
    *elem_list = malloc(sizeof(**elem_list));
    if (elem_list == NULL) {
      return -1;
    }
  }
  // Copie la clé
  (*elem_list)->key = malloc(strlen(key) + 1);
  if ((*elem_list)->key == NULL) {
    free(*elem_list);
    return -1;
  }
  memcpy((*elem_list)->key, key, strlen(key) + 1);
  // Copie la valeur
  (*elem_list)->value = malloc(elem_size);
  if ((*elem_list)->value == NULL) {
    free((*elem_list)->key);
    return -1;
  }
  memcpy((*elem_list)->value, value, elem_size);
  // Remplit le champ next et size
  (*elem_list)->size = elem_size;
  (*elem_list)->next = NULL;

  return 1;
}

static void free_hash_map(yml_hash_map *map) {
  yml_hash_map_elem *elem;
  yml_hash_map_elem *next;
  for (size_t i = 0; i < MAP_SIZE; ++i) {
    if (map->elems[i] != NULL) {
      elem = map->elems[i];
      do {
        next = elem->next;
        free(elem->key);
        free(elem->value);
        free(elem);
        elem = next;
      } while (next != NULL);
    }
  }
  free(map);
}

static size_t str_hashfun(const char *s) {
  size_t h = 0;
  for (const unsigned char *p = (const unsigned char *) s; *p != 0; ++p) {
    h = 37 * h + *p;
  }

  return h;
}
