# Project Usage Guide
This project comprises several components meant to interact with each other. Below are the instructions for each component:


## lizardsNroaches_server

### Usage:
```./lizardsNroaches_server n m```

### Parameters:
**n**: Port number for both the roaches_client and lizard_client

**m**: Port number for the display_app

### At runtime:
Define the publisher password within the server for broadcasting to the display application.


## display_app

### Usage:
```./display_app c_ip m```

### Parameters:

**c_ip**: IP address for the display application

**m**: Port number for the display application

### At runtime:
Insert the subscriber password within the display application for receiving data from the server.


## roaches_client

### Usage:
```./roaches_client c_ip n```

### Parameters:
**c_ip**: Client IP address

**n**: Port number for both the roaches_client and lizard_client

### At runtime:
Define the number of roaches within the specified range.


## lizard_client

### Usage:
```./lizard_client c_ip n```

### Parameters:
**c_ip**: Client IP address

**n**: Port number for both the roaches_client and lizard_client

