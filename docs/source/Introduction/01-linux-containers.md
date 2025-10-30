# How containers are implemented in Linux

A Linux container is a set of 1 or more processes isolated from the rest of the system. It provides resource management through control groups and resource isolation via namespaces.

Linux kernel has several types of namespaces and each of them may be considered to be a kernel feature. Let’s examine them one by one.

## PID namespaces

Provide processes with an independent set of process IDs (PIDs) separated from other namespaces. Processes inside the child PID namespace are visible from the parent PID namespace. The process with PID 8 is a direct descendant of the process with PID 6. But inside the child PID namespace they are organized in a ‘parallel’ hierarchy. The process with the PID 8 is the `init` process inside that parallel universe and, therefore, referred to as a process with PID 1. Processes with PIDs 1-3 in the child PID namespace have no knowledge of other processes' existence while the parent PID namespace processes retain the visibility on the processes with PIDs 8-10.

```mermaid
:caption: Pic 1
graph TD
    subgraph Parent PID Namespace
        A[1]
        A --> B[2]
        A --> C[3]
        B --> D[4]
        B --> E[5]
        C --> F[6]
        C --> G[7]
        G --> H[8,1]
      subgraph Child PID Namespace
        H --> I[9,2]
        H --> J[10,3]
      end
    end
```

## Mount namespace

Isolates and controls mount points. Global mountpoints view can be altered by children mount namespaces. As depicted below, the first and second child namespaces refer to the same virtual disk where their root filesystem is located, which is different from the root filesystem visible from the global (initial) mount namespace. In addition, children mount namespaces refer to different filesystems mounted on their respective `/mnt` mount points, therefore providing an individual view of the tree hierarchy for them.

```mermaid
:caption: Pic 2
graph TD
    subgraph Child Mount Namespace
        direction TB
        A["/"]
        A --> B[...]
        A --> C["/mnt"]

    subgraph Child Mount Namespace 1
        direction TB
        D["/"]
        D --> E[...]
        D --> F["/mnt"]
    end

    subgraph Child Mount Namespace 2
        direction TB
        G["/"]
        G --> H[...]
        G --> I["/mnt"]
    end
   end

    HDD --> A
    VirtualDisk1 --> G
    VirtualDisk1 --> D
    VirtualDisk2 --> F
    VirtualDisk3 --> I
```

## Network namespace

Isolates system networking resources. Global networking resources view is altered by the child net namespaces and processes in those namespaces are bestowed with a (presumingly) different set of network interfaces.

```{mermaid}
:caption: Pic 3
graph TD
   Cloud <--> A
    subgraph Global Network interfaces
         direction TB
         A["Network Interface"] <--> routing-process

      subgraph Child Network Namespace
         direction TB
         D["Network Interface"]
         D <--> E["eth1"]
         D <--> F["eth2"]
      end

      subgraph Child Network Namespace
         direction TB
         G["Network Interface"]
         G <--> H["eth1"]
         G <--> I["eth2"]
      end

      routing-process <--> D
      routing-process <--> G
          end
```

Additional namespaces exhibited by Linux include:

## Other namespaces

- **UTS namespaces** - to isolate host and  domain names, meaning that different processes may appear as running on different hosts and domains while running on the same system.
- **IPC namespaces** - to isolate inter-process communications. E.g. processes in different IPC namespaces will be able to use the same identifiers for a shared memory region and produce two such distinct regions.
- **User namespaces** - to isolate user and group ID spaces. This namespace is found useful when one needs to have the root user with ID 0 inside the namespace while the actual user ID for that user in the global namespace differs from 0\.
- **Time namespace** - to isolate machine time, allowing processes in different time namespaces to see different system times.

## Control groups (cgroups)

All the namespaces mentioned so far provide different means of resource isolation, but unless inclined to grant an unlimited amount of system resources to the processes that utilize the namespace segregation, resource accounting and limitation is required. Therefore, Linux kernel is featured with a **cgroup** mechanism providing limiting, prioritization, accounting and control features with regard to a collection of processes:

**Resource limiting** - group of processes can be set to not exceed CPU, memory, disk I/O, network limits.

**Prioritization** - some process groups may get a larger share of resources than others.

**Accounting -** measures a group's resource usage.

**Control -** facilitates freezing, check-pointing and restarting of groups of processes.

![](../images/cgroups-resource-allocation.png)

Pic 4 ([Resources allocated to the group1-web and groups2-db cgroups and associated sets of processes](https://www.oracle.com/technical-resources/articles/linux/resource-controllers-linux.html))

Control groups and resource isolation via namespaces empower isolation of processes and facilitate creation of **containers**. Containers belong to the type of virtualization also known as a ‘system level virtualization’. This type of virtualization is also called a C-type virtualization (C stands for ‘Container’). While VMs (Virtual Machines) running on top of hypervisors provide higher security level at the expense of heavier resource consumption and (to some extent) slower performance, the system level virtualization is much more lightweight (resource wise) while sacrificing several security aspects as a tradeoff.

```mermaid
:caption: Pic 5
graph TD
    subgraph Virtualization
        direction TB
        V1[APP]
        V2[APP]
        V3[APP]
        G1[GUEST OS]
        G2[GUEST OS]
        G3[GUEST OS]
        HYP[Hypervisor]
        HOST[Host Operating System]

        V1 --> G1
        V2 --> G2
        V3 --> G3
        G1 --> HYP
        G2 --> HYP
        G3 --> HYP
        HYP --> HOST
    end

    subgraph Containers
        direction TB
        C1[APP]
        C2[APP]
        SFR[Supporting Files Runtime]

        C3[APP]
        C4[APP]
        SFR2[Supporting Files Runtime]

        CHOST[Host Operating System]

        C1 --> SFR
        C2 --> SFR
        SFR2 --> CHOST
        C3 --> SFR2
        C4 --> SFR2
        SFR --> CHOST
    end
```

## Sources and other learning resources

1. cgroups(7) — Linux manual page <https://man7.org/linux/man-pages/man7/cgroups.7.html>
2. cgroupv2: Linux's new unified control group hierarchy (QCON London 2017) <https://www.youtube.com/watch?v=ikZ8_mRotT4>
3. Containerization Mechanisms: Cgroups <https://blog.selectel.com/containerization-mechanisms-cgroups/>
4. Containerization Mechanisms: Namespaces <https://blog.selectel.com/containerization-mechanisms-namespaces/>
5. Control Group v2 <https://www.kernel.org/doc/Documentation/cgroup-v2.txt>
6. RELATIONSHIPS BETWEEN SUBSYSTEMS, HIERARCHIES, CONTROL GROUPS AND TASKS <https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/6/html/resource_management_guide/sec-relationships_between_subsystems_hierarchies_control_groups_and_tasks>
7. <https://stackoverflow.com/questions/49971604/how-does-xv6-write-to-the-terminal>
8. Linux Insides - Gitbook <https://0xax.gitbooks.io/linux-insides/content/>
9. cgroups - Memory Controller <https://facebookmicrosites.github.io/cgroup2/docs/memory-controller.html>
10. Elixire Bootlin -- cgroups Memory Controller and tests
<https://elixir.bootlin.com/linux/v4.0.9/source/Documentation/cgroups/memory.txt>
<https://elixir.bootlin.com/linux/v4.0.9/source/Documentation/cgroups/memcg_test.txt>
11. LWN mailing about cgroups Memory Controller development <https://lwn.net/Articles/206697/>
12. cgroups - CPU Controller <https://www.kernel.org/doc/html/v5.4/admin-guide/cgroup-v2.html#cpu>
