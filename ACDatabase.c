/* app for different database
 *
 * support:mysql
 * */
#include "ACDatabase.h"
#include "CWConfigParse.h"

char ACSaveDatabase(char *data, char *dataType)
{
	char ret;
	if(!strcmp(dataType,"xml"))
	{
		ret = xmlSave(data);
	}

	return ret;
}

char xmlSave(char *data) {
	char ret;
	MYSQL *conn;
	MYSQL_RES *result;
	MYSQL_ROW row;
	MYSQL_FIELD *field;
	int num_fields;
	int i;

	conn = mysql_init(NULL);
	if (conn == NULL) {
		printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
		exit(1);
	}
	if (mysql_real_connect(conn, "localhost", "root", "tigercel", "ac_db", 0,
			NULL, 0) == NULL) {
		printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
		exit(1);
	}

	return ret;
}

int main(int argc, char **argv) {
	MYSQL *conn;
	MYSQL_RES *result;
	MYSQL_ROW row;
	MYSQL_FIELD *field;
	int num_fields;
	int i;

	conn = mysql_init(NULL);
	if (conn == NULL) {
		printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
		exit(1);
	}
	if (mysql_real_connect(conn, "localhost", "root", "tigercel", "testdb", 0,
			NULL, 0) == NULL) {
		printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
		exit(1);
	}
	//  if (mysql_query(conn, "create database testdb")) {
	//      printf("Error %u: %s\n", mysql_errno(conn), mysql_error(conn));
	//      exit(1);
	//  }
	//  	 mysql_query(conn, "CREATE TABLE writers(name VARCHAR(25))");
//	mysql_query(conn, "create table friends (id int not null primary key auto_increment,name varchar(20), age int)");
	//    mysql_query(conn, "INSERT INTO writers VALUES('Leo Tolstoy')");
	//    mysql_query(conn, "INSERT INTO writers VALUES('Jack London')");
	//    mysql_query(conn, "INSERT INTO writers VALUES('Honore de Balzac')");
	//    mysql_query(conn, "INSERT INTO writers VALUES('Lion Feuchtwanger')");
	//    mysql_query(conn, "INSERT INTO writers VALUES('Emile Zola')");

//	mysql_query(conn, "SELECT * FROM writers");
	mysql_query(conn, "SELECT * FROM friends");
	result = mysql_store_result(conn);
	num_fields = mysql_num_fields(result);

	while ((row = mysql_fetch_row(result))) {
		for (i = 0; i < num_fields; i++) {
			if (i == 0) {
				while (field = mysql_fetch_field(result)) {
					printf("%s ", field->name);
				}
				printf("\n");
			}
			printf("%s ", row[i] ? row[i] : "NULL");
		}
		printf("\n");
	}
	mysql_free_result(result);
	mysql_close(conn);
	return 0;

}
