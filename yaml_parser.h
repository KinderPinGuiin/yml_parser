/**
 * Interface de contrôle d'un parser de fichier yaml (.yml ou .yaml).
 * Cette implémentation sera thread safe.
 * 
 * @author Jordan ELIE.
 */

#ifndef YAML_PARSER_H
#define YAML_PARSER_H

#include <stdio.h>
#include <stdlib.h>

/*
 * Codes d'erreur
 */

#define YML_INVALID_POINTER -1
#define YML_NOT_ENOUGH_MEMORY -2
#define YML_INVALID_FILE -3
#define YML_FILE_ERROR -4
#define YML_SEM_ERROR -5
#define YML_ALREADY_EXECUTED -6

/*
 * Structure de données
 */

typedef struct yml_parser yml_parser;

/**
 * Initialise un parseur yaml et le renvoie. Renvoie NULL en cas d'erreur.
 * 
 * @param {const char *} Le chemin du fichier à parser
 * @param {int *} Un pointeur où sera stocké l'erreur s'il y en a. 
 *                Peut être nul.
 * @return {yml_parser *} Un pointeur vers le parseur en cas de succès. Renvoie
 *                        NULL en cas d'erreur.
 */
yml_parser *init_yml_parser(const char *path, int *error);

/**
 * Exécute le parseur sur le contenu de son fichier. Renvoie 1 en cas de succès 
 * et un nombre négatif sinon.
 * 
 * @param {parser *} Le parseur
 * @return {int} 1 en cas de succès et un nombre négatif en cas d'erreur.
 */
int exec_parser(yml_parser *parser);

/**
 * Récupère la valeur de la clé key dans parser et stocke sa valeur dans 
 * buffer. Renvoie 1 en cas de succès 0 si la clé n'existe pas et un nombre
 * négatif sinon.
 * 
 * @param {yml_parser *} Le parseur.
 * @param {const char *} La clé.
 * @param {void *} L'adresse où l'on souhaite stocker la valeur.
 * @return {int} 1 en cas de succès 0 si la clé n'existe pas
 *               et un nombre négatif en cas d'échec.
 */
int get(yml_parser *parser, const char *key, void *buffer);

/**
 * Libère parser ainsi que toutes ses valeurs de la mémoire. Renvoie 1 en cas 
 * de succès et une valeur négative en cas d'échec.
 * 
 * @param {yml_parser *} Le parseur.
 * @return {int} 1 en cas de succès et un nombre négatif en cas d'échec.
 */
int free_parser(yml_parser *parser);

#endif