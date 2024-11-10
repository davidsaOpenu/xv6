# Containers - Pouch
## Introduction
The purpose of the `pouch` command line utility is to create containers, to apply cgroup limitations, to get into the running container and perform operations inside of it (this operation is also called an “attachment”), to exit the attached container, to get information about the running container and, finally, to destroy it.  

For simplicity only up to 3 containers are allowed to be created by the `pouch` utility and, for the same reason, no nested containers allowed. The former limitation implied from the number of tty devices created by xv6 during the boot (3 tty devices) and the simplifying assumption that rigidly ties tty devices to the created containers. It will be a nice exercise to break this rigid dependency and to allocate tty devices only upon the attachment to a container rather than alotting them on a container creation. The latter limitation is implied from the fact that the implementation of the PID namespace, the xv6 container isolation is based on, has no support for nesting. `Pouch` utility users are able to create and destroy containers only in the "detached" mode. Only a limited set of `pouch` utility commands are available in the "attached" mode. All commands run from the shell while being in the attached mode create processes isolated from other containers. 

## Pouch commands
### Brief

The table below summarizes the supported commands according to the mode -- detached or attached.

| Detached mode | Attached mode | 
| :---- | :---- |
| `pouch start NAME` | |
| `pouch connect NAME` | `pouch disconnect` |
| `pouch info NAME`  | `pouch info` | | 
| `pouch list` | |
| `pouch destroy NAME` | |
| `pouch cgroup NAME STATE_OBJECT [VALUE, ...]` | |
| `pouch images` | |
| `pouch build --file FILE --tag IMAGE` | |
| `pouch --help` | `pouch --help` |

A successful execution of a pouch command always results in a "success" exit code of the pouch process (0).
The following sections provide a detailed description of each command.

### `pouch start`

Creates and starts a container.

***Synopsis:***  
`pouch start NAME IMAGE`
 - `NAME` -  a container identification string
 - `IMAGE` - an image name to be used for the container root filesystem

***Description:***  
Pouch containers have different pid and mount namespaces. By default no cgroup limitations are applied at this stage. Limitations have to be explicitly specified in a separate command. Nesting containers are not supported. The specified image is mounted as the root filesystem of the container, and the container is started in the background. The container is started in the detached mode. The command is available only in the detached mode. By default, the container's child process runs the `sh` binary from inside the container's root filesystem (the container's image).

### `pouch connect`

Attach user terminal to a running container using the container`s identification sting

***Synopsis:***  
`pouch connect NAME`
- `NAME` - a container identification string

***Description:***  
User terminal is connected to the tty device that is allocated to the container. The connection happens transparently to the user. The user gets a command line interface (shell) and is capable of launching processes in an isolated container`s environment. When connected, only the subset of `pouch` utility commands is available (see Tab 1).

### `pouch disconnect`
Detach the user's terminal from the running container back to the console.

***Synopsis:***

`pouch disconnect`

***Description:***  
A user will be disconnected from the running container (which he's attached to) back to the console. This command is usually used when the user is done with the container and wants to return to the console. This command is the opposite of the `pouch connect` command.

### `pouch destroy`
Stops and destroys a running container.

***Synopsis:***  
`pouch destroy NAME`
- `NAME` -  a container identification string

***Description:***  
Stops and removes a running container from the system. Detaches tty, removes a group that corresponds for the `NAME` from the cgroup filesystem. The command is available only in detached mode.

### `pouch info`
Gets information about a container and it`s state. If attached to a container, the command will return information about the attached container.

***Synopsis:***  
`pouch info [NAME]`
- `NAME` - a container identification string. Should be provided in detached mode only.

***Description:***  
Pouch info gets information about a container and it`s state. When detached from a container, the command will return information about the container specified by the `NAME` argument, the must be specified. When attached to a container, the command will return information about the attached container. Hence, the command is available in both detached and attached modes.

### `pouch list`
Get a status information about all running containers

***Synopsis:***  
`pouch list`

***Description:***  
The command gets a brief status information about all running containers. The command is available only in detached mode.

### `pouch cgroup`
limit, account or control resources associated with a container that is specified with an identification string

***Synopsis:***  
`pouch cgroup NAME [STATE_OBJECT] [ VALUE ,... ]`
 - `NAME` -  a container identification string  
 - `STATE_OBJECT`  - specified the state object name.  
 - `VALUE` - specify the value to assign to the state object. Note: xv6 shell doesn't treat a string with spaces enclosed by quotes as a single argument. Thus, multiple values have to be separated using commas (see examples below).

***Description:***  
Sets the value of a state-object (e.g. `cpu.max`) in the container's cgroup for the corresponding subsystem (e.g. `cpu`). Cpu controller is the only one cgroup controller verified at this stage. Refer to the chapter on **cgroup** for more information.

***Examples:***  
`pouch cgroup c1 cpu.max 10000` - updates cpu.max property to 10000, leaving the period default.
`pouch cgroup c1 cpu.max 10000,20000` - updates cpu.max property to 10000 and sets period to 20000\.

### `pouch images`
print a list of available images for the containers.

***Synopsis:***
`pouch images`

***Description:***
Prints a list of available images for the containers. The command is available only in detached mode. If no images are available, the command will print a message to inform the user about it.

### `pouch build`
builds an image from the specified pouchfile to a specified image name.

***Synopsis:***
`pouch build [--file pouchfile] [--name imagename]`
- `--file` - specifies the pouchfile to build the image from. If not specified, the default pouchfile is used.
- `--name` - specifies the name of the image to be built. If not specified, the default image name is used.

#### Pouchfile specification
The pouchfile format is a file format, similar to a Dockerfile, that specifies steps for the image creation process. Each steps is specified using a single command, followed by an argument(s), and each command is written on a separate line. Commands are executed in order, which changes the image's filesystem state, that is saved and passed on to the next command in the pouchfile as the build goes on.

The following commands are supported:
- `IMPORT image_name` - imports a file from the host filesystem to the image filesystem. The argument is the path to the file on the host filesystem. This command has to be the first command in the pouchfile, and it can be specified only once. After this command is executed, the new image's filesystem is populated with the same contents as the original `image_name` image's contents.
- `RUN command [arg ...]` - runs a command in the image filesystem. The argument is the command to run. This command can be specified multiple times in the pouchfile.
- `COPY file_name` - copies a file from the host filesystem to the image filesystem. The argument is the path to the file on the host filesystem, relative to the filesystem root directory (`/`). This command can be specified multiple times in the pouchfile.

Each of the commands above may fail, and the build process will stop if any of the commands fail. The build process will also stop if the pouchfile is not well-formed, i.e. if the commands are not in the correct order, or if the `IMPORT` command is specified more than once. On any fail, no image is created.

Examples for pouchfiles, both valid and invalid, can be found in the repo, under the `tests/pouchfiles` directory.

### `pouch help`
`pouch --help`  - displays all available pouch commands according to the mode (attached / detached).
