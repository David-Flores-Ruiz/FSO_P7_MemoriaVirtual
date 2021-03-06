// Definición de constantes

#define NINGUNO -1

// Definición de estructuras

struct SYSTEMFRAMETABLE {
    int assigned;

    char *paddress; // No modificar
    int shmidframe;  // No modificar
};

struct PROCESSPAGETABLE {
    int presente;
    int modificado; 
    int framenumber;
    unsigned long tarrived;
    unsigned long tlastaccess;

    int attached;   // No modificar
};

int pagefault(char *vaddress);
int countframesassigned();
int copyframe(int sframe,int dframe);
int writeblock(char *buffer, int dblock);
int readblock(char *buffer, int sblock);
int loadframe(int frame);
int saveframe(int frame);

