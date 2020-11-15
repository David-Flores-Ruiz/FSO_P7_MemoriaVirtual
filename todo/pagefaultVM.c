#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mmu.h>

#define NUMPROCS 4
#define PAGESIZE 4096
#define PHISICALMEMORY 12*PAGESIZE 
#define TOTFRAMES PHISICALMEMORY/PAGESIZE
#define RESIDENTSETSIZE PHISICALMEMORY/(PAGESIZE*NUMPROCS)

extern char *base;
extern int framesbegin;	// Inicio de marcos fisicos = 0
extern int framesend;	// Inicio de marcos virtuales = 12
extern int idproc;
extern int systemframetablesize;
extern int ptlr;

extern struct SYSTEMFRAMETABLE *systemframetable;	// Total de marcos
extern struct PROCESSPAGETABLE *ptbr;	// Tabla de páginas


int getfreeframe();
int searchvirtualframe();
int getfifo();	// Algoritmo para el reemplazo de páginas: FIFO

int pagefault(char *vaddress)
{
    int i;
    int frame,vframe; 
    long pag_a_expulsar;
    int fd;
    char buffer[PAGESIZE];
    int pag_del_proceso;

    // A partir de la dirección que provocó el fallo, calculamos la página
    pag_del_proceso=(long) vaddress>>12;
	
	frame = (ptbr+pag_del_proceso)->framenumber;
    // Si la página del proceso está en un marco virtual del disco
	if((frame > 11) && (frame != -1))	// Si no es marco fisico ni marco no asignado
    {
		vframe = frame;					// Entonces es un marco virtual
		
		// Lee el marco virtual al buffer
		readblock(buffer, vframe);
		
        // Libera el frame virtual
		systemframetable[vframe].assigned = 0;
    }


    // Cuenta los marcos asignados al proceso
    i=countframesassigned();

    // Si ya ocupó todos sus marcos, expulsa una página
    if(i>=RESIDENTSETSIZE)
    {
		// Buscar una página a expulsar (Algoritmo para reemplazo de páginas: FIFO)
		pag_a_expulsar = getfifo();
		
		// Poner el bit de presente en 0 en la tabla de páginas
        (ptbr+pag_a_expulsar)->presente = 0;
		frame = (ptbr+pag_a_expulsar)->framenumber;	// frame aqui siempre debe ser un marco fisico
        
		// Si la página ya fue modificada, grábala en disco
		if((ptbr+pag_a_expulsar)->modificado == 1)
        {
			// frame = (ptbr+pag_a_expulsar)->framenumber;	// Con la asignación aquí da ERROR
			
			// Escribe el frame de la página en el archivo de respaldo y pon en 0 el bit de modificado
			saveframe(frame);
			(ptbr+pag_a_expulsar)->modificado = 0;
		}
		
        // Busca un frame virtual en memoria secundaria
		vframe = searchvirtualframe();
		
		// Si no hay frames virtuales en memoria secundaria regresa error
		if(vframe == -1)
		{
            return(-1);
        }
		
        // Copia el frame a memoria secundaria, actualiza la tabla de páginas y libera el marco de la memoria principal
		copyframe(frame,vframe);
		(ptbr+pag_a_expulsar)->framenumber = vframe;
		systemframetable[frame].assigned = 0;
    }

    // Busca un marco físico libre en el sistema
	frame = getfreeframe();
	
	// Si no hay marcos físicos libres en el sistema regresa error
	if(frame == -1)
    {
        return(-1); // Regresar indicando error de memoria insuficiente
    }
	
    // Si la página estaba en memoria secundaria
	if(((ptbr+pag_del_proceso)->framenumber != -1) && ((ptbr+pag_del_proceso)->framenumber > 11))
    {
        // Cópialo al frame libre encontrado en memoria principal y transfiérelo a la memoria física
		writeblock(buffer, frame);
		loadframe(frame);
    }
   
	// Poner el bit de presente en 1 en la tabla de páginas y el frame 
	(ptbr+pag_del_proceso)->presente = 1;
	(ptbr+pag_del_proceso)->framenumber = frame;
	
    return(1); // Regresar todo bien
}


int getfreeframe()
{
    int i;
	
    // Busca un marco libre en los marcos fisicos: 0x00 - 0x0B
	//												0d  -  11d
    for(i=framesbegin;i<framesbegin+systemframetablesize;i++)
        if(!systemframetable[i].assigned)
        {
            systemframetable[i].assigned=1;
            break;
        }
    if(i<framesbegin+systemframetablesize)
        systemframetable[i].assigned=1;
    else
        i=-1;
    return(i);
}

int searchvirtualframe()
{
    int i;
	
    // Busca un marco libre en los marcos virtuales: 0x0C - 0x17
	//												 12d  -  23d
    for(i=framesbegin+systemframetablesize;i<framesbegin+systemframetablesize*2;i++)
        if(!systemframetable[i].assigned)
        {
            systemframetable[i].assigned=1;
            break;
        }
    if(i<framesbegin+systemframetablesize*2)
        systemframetable[i].assigned=1;
    else
        i=-1;
    return(i);
}

int getfifo()
{
	int i;
	int page;
	unsigned long tarrived_temp = -1;
	unsigned long tarrived_lowest;
	
	for(i=0;i<ptlr;i++)
		if(ptbr[i].presente == 1)	// ¿Cual página expulsar de las que están cargadas en memoria física?
		{
			if(tarrived_temp == -1)
				tarrived_temp = ptbr[i].tarrived;		// Primer tiempo encontrado
			else 
			{
				if(ptbr[i].tarrived < tarrived_temp)	// Si hay un tiempo menor guardalo en temp
					tarrived_temp = ptbr[i].tarrived;	// temp
			}
		}
	tarrived_lowest = tarrived_temp;					// lowest
	
	for(i=0;i<ptlr;i++)
		if(ptbr[i].presente == 1)
			if(ptbr[i].tarrived == tarrived_lowest)
				page = i;			// Sale la página con el menor tarrived, es decir, la primera en llegar
	
	return(page);
}
