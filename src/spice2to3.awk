#==================================================== -*- AWK -*- =====
# spice2to3.awk
#
# This awk program file is used to convert Spice2 input files to 
# Spice3 format. 
#
# Specifically, nonlinear dependent sources are converted from
# the polynomial model to the more general BXXXXX nonlinear
# dependent source model used in Spice3.
#
#
# Usage: 
#       awk -f spice2to3.awk [comments=1] inputfile > outputfile
#
#       If the optional comments=1 parameter is specified, the original 
#       input lines that are converted will be saved as comments in the
#       output file.
#
# Limitations:
#       - Cubic and higher order terms are not handled except for
#         the special case of a single variable
#       - Initial conditions are not handled (IC=...)
#       - Continued lines are not handled
#       - This program uses features from GNU awk 
#
#
# Mike McCarrick                                        Feb 1995
# mmccarri@apti.com                                     APTI, Wash DC
#======================================================================

BEGIN {
  IGNORECASE = 1;
}


#----------------------------------------------------------------------
# Utility functions
#----------------------------------------------------------------------

function min(a,b) {
  return (a < b) ? a : b;
}

function max(a,b) {
  return (a > b) ? a : b;
}


# Turn the original input line into a comment
function make_comment(inputline) {
  print "*SPICE2:";
  print "*" inputline;
  print "*SPICE3:";
}


# Get leading whitespace from the input line
function get_leader(inputline)
{
  if ((i = match(inputline, "[^ \t]")) != 0)
    return substr(inputline, 1, i-1);
  else
    return "";
}


# Get trailing comments from the input line
function get_trailer(inputline)
{
  if ((i = index(inputline, ";")) != 0)
      return substr(inputline, i, length(inputline)-i+1);
  else
    return "";
}
        

# Canonicalize the input line
function canonicalize(inputline)
{
  # Strip  parentheses and commas from the input line
  gsub(/\(/, " ", inputline);
  gsub(/\)/, " ", inputline);
  gsub(/,/,  " ", inputline);

  # chop off any trailing comments
  if ((i = index(inputline, ";")) != 0)
    inputline = substr(inputline, 1, i-1);

  return inputline;
}


# Convert numerical values to canonical form (the scale factors are
# not recognized for B devices at least in 3f4)
function convert_num(strval)
{
  sub(/T/, "E12", strval);
  sub(/G/, "E9", strval);
  sub(/MEG/, "E6", strval);
  sub(/K/, "E3", strval);
  sub(/M/, "E-3", strval);
  sub(/U/, "E-6", strval);
  sub(/N/, "E-9", strval);
  sub(/P/, "E-12", strval);
  sub(/F/, "E-15", strval);

  return strval;
}



#----------------------------------------------------------------------
# E: Voltage controlled voltage source
# G: Voltage controlled current source
# H: Current controlled voltage source
# F: Current controlled current source
#----------------------------------------------------------------------
/^ *[EGHF].+POLY/ {

  # Save the old line as a comment
  origline = $0;
  if (comments != 0)
    make_comment(origline);

  # Save any leading whitespace and trailing comments
  leader = get_leader($0);
  trailer = get_trailer($0);

  # canonicalize the input line
  $0 = canonicalize($0);

  # Print the new source name with its node connections
  printf("%sB%s %s %s ", leader, $1, $2, $3);

  # The format differs for voltage vs current controlled sources
  stype = toupper(substr($1, 1, 1));
  if (stype == "E" || stype == "H")
    printf("V = ");
  else
    printf("I = ");
  nvars = $5;
  inodes = 6;
  if (stype == "E" || stype == "G")
    {
      v_controlled = 1;
      nnodes = 2 * nvars;
    }
  else
    {
      v_controlled = 0;
      nnodes = nvars;
    }

  icoeff = inodes + nnodes;
  ncoeff = NF - icoeff + 1;
  plusflag = 0;

  # Constant term
  if ($icoeff != 0.0)
    {
      printf("%s", convert_num($icoeff));
      plusflag = 1;
    }
  icoeff++;
  ncoeff--;

  # Linear terms
  nlinear = min(ncoeff, nvars);
  for (ic = 0; ic < nlinear; ic++)
    {
      if ((val = convert_num($(icoeff + ic))) != 0.0)
        {
          if (plusflag)
              printf(" + ");
          if (v_controlled)
            {
              ip = inodes + 2 * ic;
              im = ip + 1;
              if (val != 1.0)
                printf("%s*V(%s,%s)", val, $ip, $im); 
              else
                printf("V(%s,%s)", $ip, $im); 
            }
          else
            {
              is = inodes + ic;
              if (val != 1.0)
                printf("%s*I(%s)", val, $is); 
              else
                printf("I(%s)", $is); 
            }
          plusflag = 1;
        }
    }
  icoeff += ic;
  ncoeff -= ic;


  # Quadratic terms
  nquad = min(ncoeff, nvars*(nvars+1)/2);
  for (n1 = ic = 0; n1 < nvars; n1++)
    {
      for (n2 = 0; n2 <= n1; n2++)
        {
          if (ic > nquad)
            break;

          if ((val = convert_num($(icoeff + ic))) != 0.0)
            {
              if (plusflag)
                printf(" + ");
              if (v_controlled)
                {
                  ip1 = inodes + 2 * n1;
                  im1 = ip1 + 1;
                  ip2 = inodes + 2 * n2;
                  im2 = ip2 + 1;
                  if (val != 1.0)
                    printf("%s*V(%s,%s)*V(%s,%s)", val, $ip1, $im1, $ip2, $im2);
                  else
                    printf("V(%s,%s)*V(%s,%s)", $ip1, $im1, $ip2, $im2);
                }
              else
                {
                  is1 = inodes + n1;
                  is2 = inodes + n2;
                  if (val != 1.0)
                    printf("%s*I(%s)*I(%s)", val, $is1, $is2);
                  else
                    printf("I(%s)*I(%s)", $is1, $is2);
                }
              plusflag = 1;
            }
          ic++;
        }
    }

  icoeff += ic;
  ncoeff -= ic;

  # Cubic and higher order terms are handled only for a single variable
  if (ncoeff > 0 && nvars > 1)
    {
      print "Warning: the following source line contains" > "/dev/stderr";
      print "polynomial terms greater than second order." > "/dev/stderr";
      print "Convert this line manually:" > "/dev/stderr";
      print origline > "/dev/stderr";
      # Add a warning message comment in the output file
      printf(" ; ***ERROR CONVERTING SPICE2 ENTRY\n");
      next;
    }

  # Single variable higher-order terms
  for (ic = 0; ic < ncoeff; ic++)
    {
      if ((val = convert_num($(icoeff + ic))) != 0.0)
        {
          if (plusflag)
            printf(" + ");
          if (v_controlled)
            {
              ip = inodes + 2 * ic;
              im = ip + 1;
              if (val != 1.0)
                printf("%s*V(%s,%s)^%d", val, $ip, $im, ic+3);
              else
                printf("V(%s,%s)^%d", $ip, $im, ic+3);
            }
          else
            {
              is = inodes + ic;
              if (val != 1.0)
                printf("%s*I(%s)^%d", val, $is, ic+3);
              else
                printf("I(%s)^%d", $is, ic+3);
            }
          plusflag = 1;
        }
    }

  # Add trailing comments to the output line
  printf("  %s\n", trailer);
  next;

}



#----------------------------------------------------------------------
# Default: just print out the line
#----------------------------------------------------------------------
{
  print $0;
}
