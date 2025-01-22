# Makefile for MacOS ROS Installation Guide (COMP6690)
# Usage: Run `make` to execute all steps, or `make stepX` for a specific step.

.PHONY: all step1 step2 step3 step4 step5 step6 step7 step8 step9 step10 clean

all: step1 step2 step3 step4 step5 step6 step7 step8 step9 step10

# Step 1: Install Miniconda
step1:
	@echo "Installing Miniconda..."
	@if [ "$(shell uname -m)" = "arm64" ]; then \
		mkdir -p ~/miniconda3; \
		curl https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-arm64.sh -o ~/miniconda3/miniconda.sh; \
	else \
		mkdir -p ~/miniconda3; \
		curl https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-x86_64.sh -o ~/miniconda3/miniconda.sh; \
	fi
	bash ~/miniconda3/miniconda.sh -b -u -p ~/miniconda3
	rm ~/miniconda3/miniconda.sh
	@echo "Miniconda installed!"

# Step 2: Refresh terminal
step2:
	@echo "Refreshing terminal..."
	source ~/miniconda3/bin/activate

# Step 3: Initialise conda on all available shells
step3:
	@echo "Initialising Conda..."
	source ~/miniconda3/bin/activate

# Step 4: Create ROS environment
step4:
	@echo "Creating ROS environment..."
	conda create -n ROS python=3.8 -y

# Step 5: Activate ROS environment
step5:
	@echo "Activating ROS environment..."
	conda activate ROS

# Step 6: Add conda-forge channel
step6:
	@echo "Adding conda-forge channel..."
	conda config --add channels conda-forge

# Step 7: Add robostack channel
step7:
	@echo "Adding robostack channel..."
	conda config --add channels robostack

# Step 8: Set strict channel priority
step8:
	@echo "Setting channel priority to strict..."
	conda config --set channel_priority strict

# Step 9: Install RViz and run roscore in one terminal
step9:
	@echo "Installing RViz..."
	conda activate ROS
	conda install ros-noetic-rviz==1.14.8 -y
	@echo "Starting roscore..."
	roscore &

# Step 10: Open RViz in another terminal
step10:
	@echo "Opening RViz..."
	conda activate ROS
	rviz &

# Clean up temporary files
clean:
	@echo "Cleaning up temporary files..."
	rm -rf ~/miniconda3
	@echo "Clean complete!"
