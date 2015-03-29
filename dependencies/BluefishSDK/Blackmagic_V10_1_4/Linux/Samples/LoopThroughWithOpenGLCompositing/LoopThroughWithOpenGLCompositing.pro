TEMPLATE  	= app
LANGUAGE  	= C++
CONFIG		+= qt opengl
QT			+= opengl
INCLUDEPATH =	../../include 
LIBS		+= -lGLU -ldl

HEADERS 	=	../../include/DeckLinkAPIDispatch.cpp \
				LoopThroughWithOpenGLCompositing.h \
			    OpenGLComposite.h \
			    GLExtensions.h

SOURCES 	= 	main.cpp \
            	../../include/DeckLinkAPIDispatch.cpp \
            	LoopThroughWithOpenGLCompositing.cpp \
			    OpenGLComposite.cpp \
			    GLExtensions.cpp

FORMS 		= 	LoopThroughWithOpenGLCompositing.ui
