# 0. Preface
This is an old graduation project from 2004. It was hosted on a subsection of a now defunct website, nerdclub.nl. The text and code presented here serves as a backup, for nostalgic reasons.

![](https://github.com/CatGenius/MeterMaid/raw/master/doc/images/MeterMaid.jpg)

# 1. Introduction
The "Meter Maid" in this case is not a policewoman who is assigned to write parking tickets. Instead it's a device for gathering statistics about energy consumption.

It connects to standard gas, electricity and water meters, using optical sensors. Most gas and water meters have a reflective surface at the zero-position of the least significant digit. Most electricity meters have either a spinning wheel with a darkened area on it, or an LED emitting pulses. The reflection and/or pulses are detected by the Meter Maid's optical sensors and counted by the connected microprocessor. Such a sensor can be a simple opto coupler to detect reflection, but it could also be a more complex sensor a are user in optical mice to detect and measure movement.

The Meter Maid can be operated by the user through two interfaces: The first interface is a 2 line by 40 column text LCD with four arrow pushbuttons and an enter pushbutton. By navigating though the different views, the user can read both a consumption log in different time domains as an actual load. 
The second interface is a web page. The Meter Maid can be connected to an in-house Ethernet network running the TCP/IP protocol. The user can get information about both the consumption log in a convenient table as the actual load, using a regular computer with a standard web browser. 
Later software versions will allow the user to download excel-files containing the metering data, post metering data to a facility company's website and plot graphics charts on its own website.

Sliding windows of one hour, one month and one year keep track of the most recent consumption on all three meters, with a resolution of resp. one minute, one hour and one day. This way resolution is decreased as data gets older to achieve some compression. 
At any time the average of the past 60 seconds can be view as the actual load/flow on the metered system.

Many utility companies already offer the possibility to enter consumption information on their website on a daily/weekly/monthly basis as a service to detect anomalies (leakage) at an early state. Future firmware editions of the Meter Maid may be able of submitting web forms containing the metering data autonomously. Facility companies may be interested in this product too to keep approximate track of their clients consumptions. In this case approximately, because the system can easily be tampered with. Even though tampering would be useless because of the voluntary participation. Besides that, there?s always the real meter as a tamper-free backup. The Meter Maid is not intended to replace the Meter Man :o).

# 2. Requirements
Requirements are:
* Metered pulses must be accurate; No pulses may get lost or counted more than once.
* The system must be a real time system.
* Data must be available at any time.
* The software must be event driven.
* The design must use the Model View Control principle.
* Both a metering log as an actual load must be measured.
* Three meters must be measured.
* Methods used more than once must be implemented as an object.

# 3. Considerations
The hardware platform used for the Meter Maid is Zilog's eZ80 Acclaim microcontroller on its development board. The eZ80 is a powerful 8 bit microcontroller running at 50 MHz with an on-chip Ethernet interface. The most important arguments for the choice for this platform are:
1. The eZ80 is a fairly low-cost, fairly low-power, but very powerful microcontroller with an on-chip Ethernet interface.
2. The eZ80 SDK comes complete with a full ANSI-C compliant compiler.
3. Several real time kernels are available for the eZ80. Some of which even include pre-emptive time slicers for multi-threading applications and a TCP/IP protocol stack.
4. The eZ80 SDK comes complete with an in-target debug interface (ZDI) and an in-target debugger.
The software platform used is XINU, a Compact, preemptive multitasking, multithreaded kernel with inter-process communications Support (IPC) and soft real-time attributes and a built-in TCP/IP stack. Zilog's own real time kernel is no option since it doesn't have a native TCP/IP stack. CMX would be an alternative, but it's too expensive for this project and the trial version is time bombed for one hour of operation which renders the project useless afterwards.

The development environment used for the Meter Maid is Zilog's Xtools ZDS II. It's a well equipped integrated development suite, complete with editor, compiler, linker and debugger support. An alternative would be the IAR suite but that's (way) too expensive.


# 4. Design
The design can be divided into four parts:
1. The pulse class will buffer the number of metered pulses and calculate the number of pulses per minute. It can generate a single client event on metered pulses and a multi client event on a changed number of pulses per minute.
2. The bucket memory class will listen to events of the pulse class and it will store incomming pulses into the current bucket. It will also subscribe to a real time clock event in order to change buckers. This way log is created to store the consumption statistics. The bucket memory can generate a single client event on retrieved pulses and a multi client event on change of buckets.
3. The web site class contains a html generator for a meter and a table. The meter will subscribe to changed number of pulses event of the pulse handler. The table will subscribe to the bucket change event. The generated html code is stored internally and sent to a client at request.
4. Main contains a state-machine, triggered by the keyboard, which can create and terminate lcd viewer processer. There are two kinds of viewer processes: A load viewer and a log viewer. The load viewer will subscribe to changed number of pulses event of the pulse handler and shows the current load on the lcd. The log viewer will subscribe to the bucket change event and shows the consumption statistics on the lcd. Only one of these viewers can be active at a time, so they are created and terminated dynamically as the user presses a button to select a different view.

## 4.1 Metering Model
The model drawn below shows the relationship and interaction between the pulse handler and the bucket memory. Please not that it's not UML, even though simular shapes are used.

The pulse handler uses the timer module to time stamp incomming pulses. These time stamps are queued into a circular queue in interrupt context. In process context, time stamps older that 60 seconds are dequeued. This way, the number of timestamps in the queue is always equal to the number of pulses measured in the past 60 minutes. This value will be used to calculate the current. Alternatively the time between the last two pulses could be used as well, but the major drawback of this method is that the value will 'stick' if the consumption stops. 
If the number of pulses per minute changes, an event will be generated. The number of pulses in the last 60 seconds can be retrieved at any time.

To ensure no pulses are missed or metered double, the pulse handler can notify only one client about new pulses; Incoming pulses are added to an internal buffer and an event is sent. When the client retrieves the number of pulses, this internal buffer is reset to 0. This way the system will be tolerant to missed or double handled events. In order to sent the new pulses to multiple clients, the bucket memory can be daisy chained. The bucket memory also has such an internal buffer to which retrieved pulses are added. If pulses are retrieved, the next client is notified and also this internal buffer isn't reset before the client has retrieved the pulses. This ensures the best data integrity. The last bucket memory instance doesn't have a client installed. 
The bucket memory can notify multiple clients if the buckets have been switched. The current number of buckets and eacht of the buckets contents can be retrieved at any time.

![](https://github.com/CatGenius/MeterMaid/raw/master/doc/images/MeteringModel.jpg)

## 4.2 Console Model
The model drawn below shows how thw system responds if the user presses a button.

The key handler contains the interrupt service routines for the I/O pins used for the buttons. There was no time to make the key handler fully run-time configureable, so many parameters such as the I/O port, the bit number and the debounce time have been hard coded. 
If a button is pressed, a corresponding timer is set in interrupt context. In process context, all timers are evaluated and if one expires the key event is generated. After that, the timer is set to never expire. This is a reliable way of debouncing and it prevents OS calls being made in interrupt context.

The buttenProcess in main is subscribed to the key handler and it contains a big state-machine. Each view the user can choose using the left and right button corresponds with a state. The state-machine alters it's state-number according to the key that has been pressed. If a key is pressed the current view process is terminated and the new view process is launched dynamically. Two diffent kinds of view processes ar available: One shows the actual load, the other shows the log. Given 3 meters with 3 logs for different time domains each, 12 different views are available. 
If a log view is selected, the button process subscribes itself to the up and down button as well so the user can select different log entries. Selecting a different line will also cause the log view process to be terminated, but it will be recreated with a different line as paramter. The selected line wil stay the same in relation to the current time (t-4 remains t-4 as time procedes) at the value will stay up to date. 
Besides being created and terminated, the views are also subscribed an unsubscribed to and from the events they need to update the screen. Right after creation, a fake event is sent to get initial data on the screen.

A clock process in main will show the current date and time on the first line of the display. This process is subscribed to the second event of the real time clock to refresh the displayed data.

![](https://github.com/CatGenius/MeterMaid/raw/master/doc/images/ConsoleModel.jpg)

## 4.3 Web Model
The drawing below shows how a user, the website and the measured data interact.

The website is OO designed as well, but with limited complexity. Two objects can be created: A web table and a web meter. Because the web page cannot refer to instances by using the proper instance pointer, a global lookup table is used to assign predefined reference numbers to instances. The webpage can request the proper instance by specifying the reference number as a parameter, like http://metermaid.com/table.cgi?table=01, where the reference number is 1.

The website is mostly handled by the internal webserver of the development environment. Web pages are converted to arrays of data and these arrays are specified in a structure. Besides web pages, also two cgi scripts are available. Those two scripts are specified as dynamice pages in the structure and the corresponding pointers are filled out as pointers to the handling C functions: WEB_Table and WEB_Meter. Both functions must handle the cgi request at the lowest level by replying all data to the client, including error messages. 
Also the proper reference number is retrieved from the incoming request, converted from a string to a value and translated into an instance by means of the lookup table.

A table also has a process that's subscribed to the bucket change event. If this event occurs, the number of buckets is requested and a all buckets are retrieved and translated into html code. this code is stored in a memory area allocated by the instance. 
Since a web server cannot sent new data to a client, a trick has been used to keep the client up to date: The number of second until the next whole minute is calculated and used as a refresh time for the client.

A meter works in a simular way, except it is subscribed to the event of the pulse handler and the refresh rate is fixed at 2 seconds.

![](https://github.com/CatGenius/MeterMaid/raw/master/doc/images/WebModel.jpg)

# 5. Detailed design
The next paragraphs contain short descriptions of the software modules developed for this project. Some details may already have been mentioned before.

## 5.1 Main
Main is basically the module that creates all instances and binds processes to the proper events. 
It uses few global variables to hold the instances for the pulse handlers, the bucket memories and the LC-Display. This simplifies the design an prevents complex configuration sequences with complex interfaces. Because all of these instances are globally available here, the state machine implementing the switching of views upon key events is realized here as well.

## 5.2 PHD_PulseHandler
The actual measurements of pulses are done by this module. This is a fully OO-designed module. If an instance is created, memory is dynamically allocated to store all of its properties and date. A pointer to this memory is returned to used as an instance or handle. Since many instances can coexist, a function call always has an instance as argument. This pointer is checked for its validity in all of the exported functions. Each instance spawns its own process, which is basically the same routine with a different instance pointer.

Since hardware configuration is beyond the scope of this module, it expects a function call with the proper instance for each received pulse. This function call is made by the insterrupt service routines in main.

The information the module offers is the number of pulses counted since the last time requested and the number of pulses is the last 60 seconds.

Since we’re doing metering, the number of pulses is reset after reading. This prevents pulses from being counted twice and it prevents pulses from being missed. Both situation can occur if an event is delayed or overwritten by a new one.

## 5.3 BMM_BucketMemory
Metered pulses are stored in a bucket memory. The bucket memory module is also fully OO-designed. The bucket memory acts as a log; Incoming pulses are all store in a bucket, until an event from the real time clock is received. At that time a new bucket is used to store pulses. So each bucket holds the number of pulses for a certain time frame. If all buckets are in use, the oldes bucket is emptied and recycled. Besides the number of pulses, the bucket also contains a timestamp of the time the bucket started to collect pulses. A storage process responds to the new pulses event of a meter or another bucket memory.

The current number of filled buckets and the number of pulses in each bucker can be requested by the clients to show a table of the plot a graphic chart. 
To limit the number of events and to keep the data clean, the bucket that's currently being filled is not available to the client.

XINU events offer the possibility to contain one element of data. As a convention, this element always contains the instance of the sender. This way the receiver can use it to obtain data from the proper instance very easily.

As with all modules, critical sections have been marked to guaranty thread safety.

## 5.4 WEB_Site
The internal workings of this modules have been described in paragraph 4.3

## 5.5 KEY_Handler
The internal workings of this modules have been described in paragraph 4.2

## 5.6 LCD_Driver
This module is a driver for a Hitachi controller base text LC-Display. It's not a XINU device driver, but it does implement all functionality the display has to offer.

Because the eZ80 processor doesn't allow pointers to I/O space, the specification of the I/O port isn't operational yet. At this time it defaults to Port A.

## 5.7 RTC_RealTimeClock
The real time clock is not OO-designed because there is no use of having more than one instance of it. Besides that, the MCU only contains one hardware real time clock. Even though XINU already has a real time clock, there is still need for this module. The first reason is that the XINU real time clock doesn't make use of the MCU's built-in and battery backed up real time clock. Every time XINU reboots, its real time clock is reverted back to the Epoch date of 1-1-1970. XINU does however have the possibility to synchronize to a time server on the internet, but this could take up to several minutes. Since we are logging data, an accurate real time clock is essential. XINU does not offer the option to generate an event on a time server synchronization, so the good-old polling strategy has to be used.

After initialization, the hardware real time clock is (re-) configured and a process is created. This process contains a state-machine that synchronizes the hardware real time clock to XINUs real time clock. At boot time, it will wait for the XINU real time clock to contain a plausible time, not something like 1-1-1970. After the first synchronization, the hardware real time clock is resynchronized every hour.

The second task the real time clock process performs is polling the hardware real time clock for changes and to generate events based upon them. A XINU event can only trigger one process, so if multiple processes should be notified, multiple events should be generated. A total of 15 events can be configured. Each event can either be generated every whole second, every whole minute, every whole hour or every whole day. A triggering event only occurs if the hardware real time clock has increased, not if it has only changed. This will prevent double events in case of a setback due to resynchronization. These events are required because we want to log for a calendar-day and a clock-hour, not for any period of 24 consecutive hours or 60 consecutive minutes.

Requested times are always fetched from the hardware real time clock. This way an accurate time is available at any time. Date and time rollovers are accounted for. Time is always represented as UTC. Local time and daylight savings corrections are a matter of visual representation. The RTC_DateTime_struct does however contain a Boolean to indicate if daylight savings time is applicable, but this has not been implemented yet. Nor has the calculation of the weekday. Functions to convert the number of seconds since the Epoch to a RTC_DateTime_struct and back are available. A time-zone and daylightsavingtime aware version is available too.

## 5.8 CNV_Conversions
The conversions module is basically a utillity module to easilly convert numbers to strings and back, without the use of bulky libraries. It also has some functionality implemented to retrieve words from a string with a specified separator and a case-insensitive function to check if two words are equal. These could be used to interpret readable text in for example a http GET parser.

## 5.9 TMR_Timer
The most important software unit is TMR_Timer. It is OO-designed, but it doesn't use a constructor and destructor to minimize overhead. This decision has been made because this units function calls are used very intensively by many other modules.

Instead of using a constructor, the user is responsible for allocating memory for a timer. This can be achieved by either allocating a TMR_ticks_struct on the local stack by creating a local variable, or by dynamically allocating memory through the heap manager. This 'instance' will be parsed to the functions of the module by reference. Even though the contents of the instance are exported, they should not be tinkered with. The contents could have been hidden, but the only way to do this is by declaring it differently in the header from the declaration in the source. This would lead to an extra risk of keeping the sizes of the two declarations synchronized.

The module should be initialized before any use, using TMR_Initialize. Initializing the module will reset the number of tick, hook the interrupt and initialize hardware timer 3. There is a check on a double initialization and the old interrupt vector is saved.

In the highly unlikely event that the module is no longer required, the module can be terminated. This will disable hardware timer 3 and restore the old interrupt vector. The latter action will only be done once, even if the module is terminated more than once. Termination is always successful.

Between initialization and termination the user can set a timeout, check if it has expired, postpone the timeout, force it to expire or force it to never expire. Further more, a timestamp can be set and the age of that timestamp can be requested. If a timeout has expired, requesting its age will result in a value representing how long ago it expired. Finally, hanging loops can be created for fixed delays. Please note that the timer is not set according to the macro definitions. Instead it is hard-coded to 1 millisecond per tick. In future implementations the frequency divider and reload value might be derived from these definition.
