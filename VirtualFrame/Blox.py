#!/usr/bin/env python

import sys, pygame, os, socket, threading, binascii
from pygame.locals import *
from array import *

class Blox(object):
    screen = None
    matrix = None
    cellWall = 0
    dimension = 16              #between 2 and 64
    UDP_HOST = ''
    UDP_PORT = 8888
    udp_socket = None
    running = True
    packet_size = 1024
    input_thread = None
    size = (480,480)            #Should be divisible with 'dimension'
    BLACK = 0,0,0

    def __init__(self):
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_socket.bind((self.UDP_HOST, self.UDP_PORT))   
        self.udp_socket.settimeout(1)
        
        pygame.init()
        pygame.display.init()
        self.screen = pygame.display.set_mode( self.size )
        pygame.display.set_caption("Virtual Frame")

        #Adjust the maximum packet size according to the display
        #Grid size, 2 bytes per color, 9 is a standard LMI header
        self.packet_size = self.dimension * self.dimension * 2 + 9

        self.cellWall = int(self.size[0] / self.dimension)
        self.init_matrix(self.dimension)
        
        # Clear the screen to start
        self.screen.fill(self.BLACK)
        
        # Render the screen
        pygame.display.update()   
        
        self.input_thread = threading.Thread(target=self.listen_for_input)
        self.input_thread.daemon = True
        self.input_thread.start()
        while self.running:
            self.update()
            self.draw()

        pygame.quit()
        sys.exit()

    

    def init_matrix(self, dimensions):
        self.matrix = [[self.BLACK for i in range(dimensions)] for j in range(dimensions)]
            

    def setBlock(self,x,y,color):
        self.matrix[x][y] = color

    def RGB565toRGB888(self, rgb565):
        MASK5 = 0b011111
        MASK6 = 0b111111
        b = (rgb565 & MASK5) << 3
        g = ((rgb565 >> 5) & MASK6) << 2
        r = ((rgb565 >> (5 + 6)) & MASK5) << 3
        return (r, g, b)

    def loadLMI(self, data):
        byte_order = data[3]
        header_length = data[4] + data[5]
        width = data[6] + data[7]
        height = data[8] + data[9]
        x,y = 0,0
#        print("header length:{}".format(header_length))
#        print("data length:{}".format(len(data)))
        for index in range(header_length, len(data), 2):
#            print("index:{} and data: {} {}".format(index, hex(data[index]), hex(data[index+1])))
            color_rgb565 = ((data[index] & 0xFF) << 8 ) | (data[index+1] & 0xFF)
#            print(color_uint16)
            color = self.RGB565toRGB888( color_rgb565 )
            self.matrix[x][y] = color

#            print("x:{}, y:{}".format(x, y))
            x = x + 1           
            if x == width:
                y = y+1
                x = 0
                
        pygame.display.update()
    

    def listen_for_input(self):
        while self.running:
            try:
                data, addr = self.udp_socket.recvfrom(self.packet_size)
                if data:
                    if data == b'cmd0':
                        self.udp_socket.sendto(str(self.dimension).encode("utf8"), addr)
                    if data.startswith(b'pos'):
                        x = int(chr(data[4]) + chr(data[5]), 16)
                        y = int(chr(data[6]) + chr(data[7]), 16)
                        color1 = (chr(data[8]) + chr(data[9]))
                        color2 = (chr(data[10]) + chr(data[11]))
                        color_uint16 = int((color1+color2), 16)
                        color_rgb565 = self.RGB565toRGB888(color_uint16)
#                        print("({}, {})".format(x, y))
#                        print("({},{},{})".format(r, g, b))

                        if x < len(self.matrix) and y < len(self.matrix[0]):
                            self.matrix[x][y] = color_rgb565
                    if data.startswith(b'LMI'):
                        self.loadLMI(data)
                        


                        
            except socket.error as socket_error:                
                1+1 #do nothing


    def update(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.input_thread.daemon = False
                self.input_thread = None
                pygame.quit()                
                self.running = False
                sys.exit()
    
            #Quit by pressing 'q'
            if event.type is pygame.KEYDOWN and event.key == pygame.K_q:
                self.running = False
                return

                                
        
    def draw(self):
        self.screen.fill(self.BLACK )

        for x in range(0, self.size[0], self.cellWall):
            row = int(x / self.cellWall)
            for y in range(0, self.size[1], self.cellWall):
                column = int(y / self.cellWall)
                rect = pygame.Rect(x,y, self.cellWall, self.cellWall)
                if row < len(self.matrix) and column < len(self.matrix[0]):
                    pygame.draw.rect(self.screen, self.matrix[row][column], rect)
                
        pygame.display.flip()


Blox()
