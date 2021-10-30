#!/usr/bin/perl

use strict;
use warnings;

open FH, "GLExtensionDefs.txt" or die $_;

my$ output = "// This file is automatically generated with Runtime/GfxDevice/opengl/GenerateGLExtensionDef.pl.\n// It is generated from GLExtensionDefs.txt\n";

LINE: 
while (<FH>)
{	
	my $line = $_;
	chomp ($line);
	
	if ($line =~ /^\s*\/\/\s*(.*)/)
	{
		$output = $output . "$line\n";
	}
	elsif ($line =~ /^\s*\#\s*(.*)/)
	{
		$output = $output . "$line\n";
	}
	elsif ($line =~ /^\s* s*(.*)/)
	{
		$output = $output . "$line\n";
	}
	elsif ($line =~ /^\s*$/)
	{
		$output = $output . "$line\n";
	}
	else
	{
		my$ name = $line;
		my$ pfn = uc ($line);
		if ($line =~ /^(.+)->(.+)/)
		{
			$name = $1;
			$pfn = uc ($2);
		}
		my $upperLine = uc ($line);
		$output = $output . "DEF (PFN" . $pfn . "PROC, $name);\n";
		$output = $output . "#define $name UNITYGL_$name\n";
	}
}

open OUT, "> GLExtensionDefs.h";
print OUT $output;