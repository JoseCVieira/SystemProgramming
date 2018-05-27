/*H***********************************************************************************
* FILENAME :        clipboard.h
*
* DESCRIPTION :
*       File distributed clipboard interaction routines. 
*
* PUBLIC FUNCTIONS :
*       int clipboard_connect(char * clipboard_dir);
*       int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);
*       int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);
*       int clipboard_wait(int clipboard_id, int region, void *buf, size_t count);
*       void clipboard_close(int clipboard_id);
*
* NOTES :
*
*
* AUTHOR :    José Carlos Vieira        START DATE :    June 2018
* AUTHOR :    Pedro Carmo
*
* CHANGES :
*
*H*/



/**
* DESCRIPTION :
*       This function is called by the application before interacting with the distributed clipboard.
* 
* ARGUMENTS :
*       clipboard_dir - this argument corresponds to the directory where the local clipboard was launched.
*       
* RETURN VALUE :
*       The function return -1 if the local clipboard can not be accessed and a positive value in
*       case of success. The returned value will be used in all other functions as clipboard_id.
*/

int clipboard_connect(char * clipboard_dir);


/**
* DESCRIPTION :
*       This function copies the data pointed by buf to a region on the local clipboard.
* 
* ARGUMENTS :
*       clipboard_id - this argument corresponds to the value returned by clipboard_connect.
*       region - This argument corresponds to the identification of the region the user wants 
*                to copy the data to. This should be a value between 0 and 9.
*       buf - pointer to the data that is to be copied to the local clipboard.
*       count - the length of the data pointed by buf.
* 
* RETURN VALUE :
*       This function returns positive integer corresponding to the number of bytes copied or 0 in
*       case of error (invalid clipboard_id, invalid region or local clipboard unavailable).
*/

int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);


/**
* DESCRIPTION :
*       This function copies from the system to the application the data in a certain region. The
*       copied data is stored in the memory pointed by buf up to a length of count.
* 
* ARGUMENTS :
*       clipboard_id - this argument corresponds to the value returned by clipboard_connect.
*       region - This argument corresponds to the identification of the region the user
*                wants to paste data from. This should be a value between 0 and 9.
*       buf - pointer to the data where the data is to be copied to.
*       count - the length of memory region pointed by buf.
* 
* RETURN VALUE :
*       This function returns a positive integer corresponding to the number of bytes copied or 0 in
*       case of error (invalid clipboard_id, invalid region or local clipboard unavailable).
*/

int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);


/**
* DESCRIPTION :
*       This function waits for a change on a certain region ( new copy), and when it happens the
*       new data in that region is copied to memory pointed by buf. The copied data is stored in
*       the memory pointed by buf up to a length of count.
* 
* ARGUMENTS :
*       clipboard_id - this argument corresponds to the value returned by clipboard_connect.
*       region - This argument corresponds to the identification of the region the user
*                wants to paste data from. This should be a value between 0 and 9.
*       buf - pointer to the data where the data is to be copied to.
*       count - the length of memory region pointed by buf.
* 
* RETURN VALUE :
*       This function returns a positive integer corresponding to the number of bytes copied or 0 in
*       case of error (invalid clipboard_id, invalid region or local clipboard unavailable).
*/

int clipboard_wait(int clipboard_id, int region, void *buf, size_t count);


/**
* DESCRIPTION :
*       This function closes the connection between the application and the local clipboard.
* 
* ARGUMENTS :
*       clipboard_id - this argument corresponds to the value returned by clipboard_connect.
* 
* RETURN VALUE :
*/

void clipboard_close(int clipboard_id);
