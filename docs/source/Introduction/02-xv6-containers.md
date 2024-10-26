# How containers are implemented in xv6

Like in Linux, xv6 container is an instance facilitating an isolation of the xv6 operating system resources, their accounting, limitation and control. Since xv6 has limited amount of features the isolation is based on PID and mount namespaces augmented with cgroup mechanism.  Userspace command line utility called ‘pouch’ lets xv6 users easily create and manage xv6 containers. 

In the following section the pouch utility, cgroup, PID and mount namespaces are described from users perspective while the chapter next to it dives into the implementation details. 

