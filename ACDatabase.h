/* header for different database
 *
 * support:mysql
 * */

//AC
#include "CWAC.h"
#include "CWVendorPayloads.h"
#include "CWFreqPayloads.h"

//client(web,...)
#include "WUM.h"


//mysql
#include <my_global.h>
#include <mysql.h>



char ACSaveDatabase(char *data);
