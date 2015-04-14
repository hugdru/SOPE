all: default
default: debug

release:
	+$(MAKE) -C index release
	+$(MAKE) -C sw release
	+$(MAKE) -C csc release
debug:
	+$(MAKE) -C index debug
	+$(MAKE) -C sw debug
	+$(MAKE) -C csc debug

clean:
	+$(MAKE) -C index clean
	+$(MAKE) -C sw clean
	+$(MAKE) -C csc clean
