Introduction
============

Container technologies popularity increased through the past decade and this tendency is expected to continue. The influence of containers is prominent in many ways spanning every aspect of the programming product lifecycle from the design and architecture considerations, followed by the programming language adoption and ending with the product deployment and production environment maintenance. 

Looking under the hood to understand how containers are implemented, being familiar with the operating system features that container technologies rely on, is compelling. This awareness is also significant in order to assess all the pros and cons at early stages of a product design. Looking at the implementation of the operating system’s features necessary for containers is much easier based on operating systems intended for educational use. One of them, the [xv6](https://en.wikipedia.org/wiki/Xv6), is simple enough, yet contains the important concepts and organisation of a Unix like operating system. As for 2021, Linux code contains 31M lines of code (~14% of them is the kernel code) compared with only 18K in xv6, thus going from ‘simple to complex’ seems to be a desired stage.

This whitepaper describes the way containers were added to the xv6 operating system. The design and user interface was formed based on the Linux operating system. Therefore, we will start from a general overview of the features that container implementations depend on in the Linux operating system and then continue with the description of the functional and technical details related to the feature subset used for the lean containers implementation in xv6. 

