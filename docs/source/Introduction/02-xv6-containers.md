# How containers are implemented in xv6

Like in Linux, xv6 container is an instance facilitating an isolation of the xv6 operating system resources, their accounting, limitation and control. Since xv6 has limited amount of features the isolation is based on PID and mount namespaces augmented with cgroup mechanism. Userspace command line utility called ‘pouch’ lets xv6 users easily create and manage xv6 containers. 

In the [functional specification section](../FunctionalSpecification/01-namespaces.md) the pouch utility, cgroup, PID and mount namespaces are described from users perspective while the chapter next to it, the [technical specification](../TechnicalSpecification/01-namespaces.md), dives into the implementation details. 

This document collection is the second version of the original, [xv6 cgroup, namespaces and containers](https://docs.google.com/document/d/1iLGOXVAlkl9Ee2VLVfnYWHt4efNy5_NapldoVtK4Nzo/edit?tab=t.0#heading=h.qlemqf813ps) document, that has been splitted and improved to match the new structure of the xv6 documentation.