# SystemProgramming

Distributed clipboard, implemented in C for System Programming course.

### Description:

Applications can copy and past to/from the distributed clipboard using a predefined API. 
The distributed clipboard will run on seversal machines, where
data copied to one computer is be available to be pasted on another computer.
The architecture of the distributed clipboard is composed of various components, as
follow:

<p align="center">
  <img src="https://i.imgur.com/t7Od41d.png">
</p>

### Architecture:

The components of the system are:
- API - set of interfaces that allow programmers to develop applications that use the distributed clipboard.
- Library - implementation of the API, with the code for the application to interact with the clipboard.
- Local Clipboard - component responsible for receiving connections from the local
applications and the clipboards running on different computers (remote clipboards).
This component receives the commands from the applications, stores the data and
replicate it to other remote clipboards.


### Overall functionality:

When an application starts, it first needs to connect to the local clipboard, from that
moment on the application can issued copies and pastes. If the local clipboard is not
running, the initialization should return an error.

The local clipboard can be started in single mode or connected to another one. In the
case that the clipboard is connected to another, all copy commands on either clipboard
are replicated to the other.

The distributed clipboard holds 10 different regions that can be accessed
independently. Whenever a values is copied to a region, the old value in that region is
deleted and this new value is also replicated to all the other remote clipboards.

### API:

In order to implement applications the system provide an API composed of the following functions:

#### int clipboard_connect(char * clipboard_dir)
This function is called by the application before interacting with the distributed clipboard.

	- Arguments:
		- clipboard_dir – this argument corresponds to the directory where the local clipboard was launched.
	- Return: 
		-  -1 if the local clipboard can not be accessed and a positive value in case of success.
		
#### int clipboard_copy(int clipboard_id, int region, void *buf, size_t count)
This function copies the data pointed by buf to a region on the local clipboard.

	- Arguments:
		- clipboard_id – this argument corresponds to the value returned by clipboard_connect
		- region – This argument corresponds to the region the user wants to copy the data to.
		- buf – pointer to the data that is to be copied to the local clipboard
		- count – the length of the data pointed by buf.
	- Return: 
		-  This function returns positive integer corresponding to the number of bytes copied or 0 in
		case of error (invalid clipboard_id, invalid region or local clipboard unavailable).
		
#### int clipboard_paste(int clipboard_id, int region, void *buf, size_t count)
This function copies from the system to the application the data in a certain region. The
copied data is stored in the memory pointed by buf up to a length of count.

	- Arguments:
		- clipboard_id – this argument corresponds to the value returned by clipboard_connect
		- region – This argument corresponds to the identification of the region the user wants to wait for.
		- buf – pointer to the data where the data is to be copied to
		- count – the length of memory region pointed by buf.
	- Return: 
		-  This function returns a positive integer corresponding to the number of bytes copied or 0 in
		case of error (invalid clipboard_id, invalid region or local clipboard unavailable).

#### int clipboard_wait(int clipboard_id, int region, void *buf, size_t count)
This function waits for a change on a certain region ( new copy), and when it happens the
new data in that region is copied to memory pointed by buf. The copied data is stored in
the memory pointed by buf up to a length of count.

	- Arguments:
		- clipboard_id – this argument corresponds to the value returned by clipboard_connect
		- region – This argument corresponds to the identification of the region the user wants to wait for
		- buf – pointer to the data where the data is to be copied to
		- count – the length of memory region pointed by buf
	- Return: 
		-  This function returns a positive integer corresponding to the number of bytes copied or 0 in
		case of error (invalid clipboard_id, invalid region or local clipboard unavailable).

#### void clipboard_close(int clipboard_id)
This function closes the connection between the application and the local clipboard.

	- Arguments:
		- clipboard_id – this argument corresponds to the value returned by clipboard_connect


### Contributors:
	
	-Name: 		José Carlos Vieira
	-e-mail:	josecarlosvieira@tecnico.ulisboa.pt
	-Degree: 	MEEC

	-Name:		Pedro Esperança do Carmo
	-e-mail:	pedro.carmo@tecnico.ulisboa.pt
	-Degree:	MEEC

### Institution:

	-Instituto Superior Técnico, Universidade de Lisboa (2017/2018)
