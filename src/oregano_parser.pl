#!/usr/bin/perl
#
# Parser de ngSpice a GNU Cap para Oregano
#
# Utilidad : Hacer que la salida del ngSpice sea como la del
# GNUCap (porque es mas simple de parsear) para no tener
# que diferenciar el engine desde la GUI
#

# Los parametros los espera de la siguiente forma
#
# #> oregano_parser.pl engine netlist
#
# engine : gnucap o ngspice


use Switch;
use strict;

# Leo parametros
my $engine = shift;
my $netlist = shift;

# Capturo la salida del engine
my $data = `$engine -b $netlist`;

if ($engine eq "gnucap") {
	my $dc_done;
	$dc_done = 0;
	# Oregano ya sabe que hacer con GNU Cap
	# Solo imprimo la salida ;-)
	#
	# TODO : Necesito modificar la salida del Analisis DC
	# para cambiar el # por un #DC para compatibilidad
	# de ambos engines.
	my @lineas = split(/\n/, $data);

	# Ugly hack to disingue DC from OP
	my $buffer;
	my $dc_line;
	my $op_line;
	$buffer = '';
	foreach my $linea (@lineas) {
		if (($linea =~ /# /) and ($dc_done == 0)) {
			$dc_done = 1;
			$dc_line = $linea;
			next;
		}
		if (($linea =~ /# /) and ($dc_done == 1)) {
			$dc_done = 2;
		}

		if ($dc_done == 0) {
			print $linea."\n";
		} elsif ($dc_done > 0) {
			$buffer .= $linea."\n";
		}
	}
	if ($dc_done == 2) {
		$dc_line =~ s/# /#DC/g;
		print $dc_line."\n";
		print $buffer;
	} elsif ($dc_done == 1) {
		print $dc_line."\n";
		print $buffer;
	}
	exit(0);
}

#Separo en lineas:
my @lineas = split(/\n/, $data);

# Variable para saver en que analisis estoy
my $en_analisys = 0;

my $linea;
my $dato;
my @datos;
my %ac_out;
my @ac_var;
my $ac_current_var;
my $ac_var_count;
my $ac_data_count;
my $hay_ac;

$hay_ac = 0;
$ac_var_count = 0;
$ac_data_count = 0;
foreach $linea (@lineas) {
	if ($linea =~ /Transient Analysis/) {
		$en_analisys = 1; # Estoy en analisis de Tiempo!
	}
	if ($linea =~ /AC Analysis/) {
		$en_analisys = 2; # Estoy en analisis de AC!
		$hay_ac = 1;
	}
	if ($linea =~ /DC transfer/) {
		$en_analisys = 3; # Estoy en analisis de DC!
	}

	switch ($en_analisys) {
		case 1 {
			&ngspice_tran($linea);
		};
		case 2 {
			&ngspice_ac($linea);
		};
		case 3 {
			&ngspice_dc($linea);
		}
	}
}

if ($hay_ac) {
	&ac_print();
}

sub ngspice_ac()
{
	$linea = shift;
	if ($linea =~ /Index/) {
		#Tengo el Titulo!
		@datos = split(/ /, $linea);
		foreach $dato (@datos) {
			if ($dato ne "") {
				if ($dato =~ /v/) {
					# Si es v( ) la marco como variable actual
					$ac_current_var = $dato;
					push @ac_var, $dato;
					$ac_var_count++;
				}
			}
		}
	} else {
		if ($linea =~ /\d\t\d/) {
			@datos = split(/\t/, $linea);
			shift @datos; # Saco la columna Index
			# Guardo frequency
			# Gaurdo el valor de la variable actual
			push @{$ac_out{$ac_current_var}}, $datos[2];
			push @{$ac_out{freq}}, $datos[0];
			$ac_data_count++;
		}
	}
}

sub ngspice_tran()
{
	$linea = shift;
	if ($linea =~ /-------/) {
		#Ignoro esta linea molesta
	} else {
		if ($linea =~ /Index/) {
			#Tengo el Titulo!
			@datos = split(/ /, $linea);
			foreach $dato (@datos) {
				if ($dato ne "" && $dato ne "time") {
					if ($dato eq "Index") {
						print "#Time\t";
					} else {
						print $dato."\t";
					}
				}
			}
			print "\n";
		} else {
			if ($linea =~ /\d\t\d/) {
				@datos = split(/\t/, $linea);
				shift @datos;
				foreach $dato (@datos) {
					print $dato."\t";
				}
				print "\n";
			}
		}
	}
}

sub ngspice_dc()
{
	$linea = shift;
	if ($linea =~ /-------/) {
		#Ignoro esta linea molesta
	} else {
		if ($linea =~ /Index/) {
			#Tengo el Titulo!
			@datos = split(/ /, $linea);
			foreach $dato (@datos) {
				if ($dato ne "" && $dato ne "v-sweep") {
					if ($dato eq "Index") {
						print "#DC\t";
					} else {
						print $dato."\t";
					}
				}
			}
			print "\n";
		} else {
			if ($linea =~ /\d\t\d/) {
				@datos = split(/\t/, $linea);
				shift @datos;
				foreach $dato (@datos) {
					print $dato."\t";
				}
				print "\n";
			}
		}
	}
}

sub ac_print()
{
	my @key1, $dato;
	my $key;
	my $var;
	my $i;
	print "#Freq\t";
	# Imprimo el header de la tabla
	foreach $key (@ac_var) {
		print $key."\t";
	}
	print "\n";

	# Imprimo los valores de la tabla
	for($i=0; $i<$ac_data_count/$ac_var_count; $i++) {
		print $ac_out{freq}[$i]."\t";
		foreach $key (@ac_var) {
			print $ac_out{$key}[$i]."\t"
		}
		print "\n"
	}
}

