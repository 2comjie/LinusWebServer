CXX = g++
CXXFLAGS = -std=c++14
LDFLAGS = -lmysqlclient
# 源文件
SRCS =	buffer/buffer.cpp \
		http/httpconn.cpp http/httprequest.cpp http/httpresponse.cpp \
		log/log.cpp \
		pool/sqlconnpool.cpp pool/threadpool.h \
		server/epoller.cpp server/webServer.cpp \
		timer/heaptimer.cpp \
		main.cpp

main:$(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) 
.PHONY: clean
clean:
	rm -f main