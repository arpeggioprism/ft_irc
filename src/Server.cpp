#include "../include/Server.hpp"
#include "../include/Client.hpp"

Server::Server(int port, std::string &password) : _port(port), _password(password), cmdManager(_clientManager, _channelManager, password) {}
Server::~Server() {}

void Server::close_all_fd(void)
{
	std::vector<Client> clients = _clientManager.get_connect_clients();
	for (size_t i = 0; i < clients.size(); i++)
	{
		close(clients[i].getClientFd());
	}
	close(_serverSocket);
	std::cout << "close all fd" << std::endl;
}

static std::string recvMessage(Client &client)
{
	//질문! 함수 내부의 static 변수는 1번만 초기화되고 이후 호출될 때마다 변하는 값을 함수 내부에서 갖고 있으려고 선언하는건데.. 왜 memset을 계속 해주는걸까?
	static char buf[BUF_LEN];
	std::memset(buf, 0, sizeof(buf));

	//recv로 읽은 결과 byte가 0일때 예외인가 정상처리인가?
	ssize_t	 byte;
	if ((byte = recv(client.getClientFd(), buf, sizeof(buf), 0)) < 0 || (byte > BUF_LEN))
		throw std::exception();
	
	std::cout << GREEN << "From Client " << client.get_nick() << RESET << ": " << buf << std::endl;
	
	return buf;
}

void Server::receiveCommand(Client &sender)
{
	sender.commandBuffer.appendToBuf(recvMessage(sender));
	while (sender.commandBuffer.hasCommand())
		cmdManager.executeCommand(sender, sender.commandBuffer.makeCommand());
}

void Server::connectNewClient(void)
{
	int clientFd = -1;
	while (clientFd == -1)
	{
		if ((clientFd = accept(_serverSocket, NULL, NULL)) < 0)
			throw std::exception();
		else
		{
			std::cout << YELLOW << "Socket fd " << clientFd << " accepted" << RESET << std::endl;
			addNewPoll(clientFd);
			_clientManager.addClient(clientFd);
		}
	};
}

void Server::addNewPoll(int socketFd)
{
	struct pollfd newPollFd;

	newPollFd.fd = socketFd;
	newPollFd.events = POLLIN;
	newPollFd.revents = 0;

	_pollFds.push_back(newPollFd);
}

//패스워드 틀리는 경우 pollfd가 erase되지 않는 문제 존재. 이후 패스워드 맞게되면 poll은 +1개가 된다.
void Server::PollLoop(void)
{
	while (1)
	{
		//std::cout << "pollfd count: " << _pollFds.size() << std::endl;
		if (poll(&_pollFds[0], _pollFds.size(), TIMEOUT) == -1)
			throw std::exception();
		
		for (size_t i(0); i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].revents == 0)
				continue;
			
			if (_pollFds[i].revents & (POLLERR | POLLHUP))
			{
				_clientManager.deleteClient(_clientManager.getClientByFd(_pollFds[i].fd), _channelManager);
				_pollFds.erase(_pollFds.begin() + i);
			}
			else if (_pollFds[i].revents & POLLIN)
				(_pollFds[i].fd == _serverSocket) ? connectNewClient() : receiveCommand(_clientManager.getClientByFd(_pollFds[i].fd));
		}
	}
}

void Server::createServerSocket(void)
{
	if ((_serverSocket = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		throw std::exception();
	
	int enable = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
		throw std::exception();
	
	if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) < 0)
		throw std::exception();
	
	struct sockaddr_in serverInfo;
	std::memset(&serverInfo, 0, sizeof(serverInfo));
		
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_addr.s_addr = INADDR_ANY;
	serverInfo.sin_port = htons(_port);
	
	if (bind(_serverSocket, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) < 0)
		throw std::exception();
	
	if (listen(_serverSocket, SOMAXCONN) < 0)
		throw std::exception();
	
	std::cout << YELLOW << "ft_IRC is now running.........." << RESET << std::endl;
}

void Server::run(void)
{
	createServerSocket();
	addNewPoll(_serverSocket);
	PollLoop();
}
