# Sidekick EPICS

This project is a framework for rapid prototyping new ideas in control, DAQ, and machine learning for high-intensity laser systems. You can build this "sidekick" system in your own office, connect to it from your laptop, and gain familiarity with EPICS.

Once you've gained familiarity, the setup is extremely modular – re-made in a variety of forms to suit the research goals of whoever is using it.  This low-stakes system is built with the same exact EPICS architecture that we plan to implement in laser-plasma laboratory facilities. The system as posted here has custom features tailored to our real-world laser laboratory experience. Just like in a laser laboratory, all elements are triggered from an external timing box. And just like in a laser laboratory, there are triggered flashes of light (in our case, LEDS), and gated detectors (in our case, phototransistors). Think of it like an office-safe, very-much slowed-down version of a minimal laser experiment, with lots of hooks into the same EPICS control settings that you'll need to learn about for the real laboratory setups.

The architecture of this "sidekick" test system would go alongside a real laboratory system based in the same EPICS infrastructure. The "sidekick" system is based on Arduinos and Raspberry Pis, so as to be very approachable, rather than high-performance. Most importantly, this setup can be bought for under $300 and set up on an office desk.
 
We think building small test setups in EPICS, could accelerate our scholarly progress, help us communicate our science, be a great opportunity for student involvement, and even be quite a bit of fun. We hope you enjoy and learn from getting your feet wet with this "sidekick" EPICS project.

Created by Dr. Scott Feister and undergraduates Keily Valdez-Sereno and Emiko Ito, of California State University Channel Islands, and in collaboration with researchers in the Cognitive Simulation Team at Lawrence Livermore National Laboratory.