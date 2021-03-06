# ---------------------------------------------------------------------------
# Makefile for Java EditLine interface
#
# Copyright (c) 2010 Brian M. Clapper, bmc@clapper.org
# ---------------------------------------------------------------------------

PLATFORM     = $(shell uname -s)
CLASSDIR     = classes
CLASS_PKGDIR = $(CLASSDIR)/org/clapper/editline
CLASSES      = $(CLASS_PKGDIR)/EditLine.class
APIDOCS      = ../apidocs
SOLIB_NAME   = javaeditline
JAR_NAME     = javaeditline.jar

# ---------------------------------------------------------------------------
# Platform-specific stuff
# ---------------------------------------------------------------------------

include Makedefs.$(PLATFORM)

# ---------------------------------------------------------------------------

JAVAC    = $(JAVA_HOME)/bin/javac
JAVAH    = $(JAVA_HOME)/bin/javah
JAR      = $(JAVA_HOME)/bin/jar
JAVADOC  = $(JAVA_HOME)/bin/javadoc

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

all: compile package

clean:
	rm -rf $(CLASSDIR) $(SOLIB) *.o \
        org_clapper_editline_*.h *.jar *.class $(APIDOCS)

docs:
	$(JAVADOC) -d $(APIDOCS) \
        -header '<b>Java EditLine</b>' \
	-footer 'Copyright &copy; 2010 Brian M. Clapper' \
        EditLine.java

compile: java native

package: jar

jar: $(JAR_NAME)

$(JAR_NAME): $(CLASSES)
	(cd $(CLASSDIR); $(JAR) cf ../$(JAR_NAME) org)

java:	$(CLASSES)

native: $(SOLIB)

test: TestDriver.class

$(CLASS_PKGDIR)/EditLine.class: EditLine.java
	mkdir -p $(CLASSDIR)
	$(JAVAC) -d $(CLASSDIR) -cp $(CLASSDIR) EditLine.java

$(SOLIB): org_clapper_editline_EditLine.o
	$(CC) -o $(SOLIB) org_clapper_editline_EditLine.o $(LDFLAGS) 

org_clapper_editline_EditLine.o: org_clapper_editline_EditLine.c \
                                 org_clapper_editline_EditLine.h
	$(CC) $(INCLUDES) $(CFLAGS) -c org_clapper_editline_EditLine.c

org_clapper_editline_EditLine.h: $(CLASS_PKGDIR)/EditLine.class
	$(JAVAH) -classpath $(CLASSDIR) -jni org.clapper.editline.EditLine

TestDriver.class: TestDriver.java $(JAR_NAME)
	$(JAVAC) -d . -cp $(CLASSDIR) TestDriver.java
