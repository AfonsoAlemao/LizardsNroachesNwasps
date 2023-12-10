# Project Usage Guide
This project comprises several components meant to interact with each other. Below are the instructions for each component:


## lizardsNroaches_server

### Usage:
```./lizardsNroaches_server <client-port> <display-port>```

### Parameters:
**client-port**: Port number for both the roaches_client and lizard_client

**display-port**: Port number for the display_app

### At runtime:
Define the publisher password within the server for broadcasting to the display application.


## display_app

### Usage:
```./display_app <display-address> <display-port>```

### Parameters:

**display-address**: IP address for the display application

**display-port**: Port number for the display application

### At runtime:
Insert the subscriber password within the display application for receiving data from the server.


## roaches_client

### Usage:
```./roaches_client <client-address> <client-port>```

### Parameters:
**client-address**: Client IP address

**client-port**: Port number for both the roaches_client and lizard_client

### At runtime:
Define the number of roaches within the specified range.


## lizard_client

### Usage:
```./lizard_client <client-address> <client-port>```

### Parameters:
**client-address**: Client IP address

**client-port**: Port number for both the roaches_client and lizard_client

### At runtime:
Control the assigned lizard with the arrow keys. To disconnect from the game press "Q" or "q".
