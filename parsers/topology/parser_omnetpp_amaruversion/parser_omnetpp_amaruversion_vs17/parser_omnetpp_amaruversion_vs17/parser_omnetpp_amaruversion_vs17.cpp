#include "pch.h"
#include "parser_funciones.h"



int main() {
	int z = 1, i, num_topo, status, aux_parser; //var aux
	char name_dir[50]; //Name of root Dir.
	char **name_dir_dynamic = NULL; // pointer to make a dynamic reserve
	char command_string[50]; //var aux
	char path[PATH_MAX];     // path 
	FILE *fp = NULL;      //pointer pipe

	//Ask for the name of root dir.
	NameDir(name_dir);

	//Num. Topos.
	num_topo = NumTopos(name_dir);

	printf("\n\n %d Topologies detected..\n\n", num_topo);
	system("pause");

	//Dynamic reservation of the two - dimensional array for the names of each topology
	name_dir_dynamic = (char **)malloc(num_topo * sizeof(char *));
	if (name_dir_dynamic == NULL) {
		system("cls");
		printf("Error there is not enough space in the memory...");
		system("pause");
		exit(-1);
	}
	for (i = 0; i < num_topo; i++) {
		name_dir_dynamic[i] = (char *)malloc(50);

	}

	//We build the command to list the directories of our root directory
	strcpy_s(command_string, "cd ");
	strcat_s(command_string, name_dir);
	strcat_s(command_string, " && dir");


	fp = _popen(command_string, "r");                  //Open pipe to get as input the output of the prompt to the DIR command within the
	if (fp == NULL)                                    //   root directory.
		exit(-1);

	i = -2;
	while (fgets(path, PATH_MAX, fp) != NULL) {

		z = contenString(path);                      //Checks each line to verify if is a directory. 

		if (z == 1) {

			ListDir(path, name_dir_dynamic[i]);      //Discriminates:  <DIR> .\n  and  <DIR> ..\n
			i++;
		}
	}


	//Parser starts
	for (i = 0; i < num_topo; i++) {
		aux_parser = Parser(name_dir_dynamic[i], name_dir);

		if (aux_parser == 0) {
			printf("\nError in the parsing of the topology %s \n", name_dir_dynamic[i]);
		}
	}

	//Free pipe
	status = _pclose(fp);
	if (status == -1) {
		exit(-1);
	}

	//Free dynamic reserve 
	for (i = 0; i < num_topo; i++) {

		free(name_dir_dynamic[i]);
	}
	free(name_dir_dynamic);

	printf("\n\n\n ---- Work finished ----\n\n\n");
	system("pause");

	return 0;
}
