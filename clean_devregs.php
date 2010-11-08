#!/usr/bin/php
<?php

/* line types */
$COMMENT    = 0 ;
$REGISTER   = 1 ;
$FIELD      = 2 ;
$PARSEERR   = 3 ;

$line_types = Array('COMMENT','REGISTER','FIELD','PARSEERR');

/* 
 * Returns line type and value if !COMMENT
 */
function parseLine($line,&$value) {
      global $COMMENT ;
      global $REGISTER ;
      global $FIELD ;
      global $PARSEERR ;
      $p = strpos($line,'#');
      $value['line'] = $line ;
      $value['type'] = $PARSEERR ;
      if (FALSE !== $p) {
         $line = substr($line,0,$p);
      }
      $line = strtoupper(trim($line));
      if (0 == strlen($line)) {
         $value['type'] = $COMMENT ;
      } else if (':' == $line[0]) {
         $line = substr($line,1);
         $parts = explode(':',$line);
         if (2 == count($parts)) {
            $value['name'] = $parts[0];
            $value['bits'] = $parts[1];
            $bits = explode('-',$parts[1]);
            if ((1 == count($bits))||(2 == count($bits))) {
               foreach($bits as $bitnum) {
                  if (!is_numeric($bitnum)) {
                     print ("invalid bitfield $bitnum on line $line\n");
                     return $PARSEERR ;
                  }
               }
               $value['startbit'] = $bits[0];
               $value['stopbit'] = (2 == count($bits)) ? $bits[1] : $bits[0];
               if ($value['startbit'] > $value['stopbit']) {
                  $tmp = $value['startbit'];
                  $value['startbit'] = $value['stopbit'];
                  $value['stopbit'] = $tmp;
               }
               $value['type'] = $FIELD ;
            } else
               print( "invalid bitfield on line $line\n" );
         } else {
            print ("field $line missing bitfield\n");
         }
      } else if (('_' == $line[0]) || (('A' <= $line[0])&&('Z' >= $line[0]))) {
         $parts = $value['parts'] = sscanf($line,"%s %s");
         if (2 == count($parts)) {
            $value['name'] = $parts[0];
            $address = $parts[1];
            $parts = explode('.',$address);
            if (2 > count($parts))
               $parts[] = 'L' ;
            $value['address'] = strtoupper($parts[0]);
            $value['width'] = $parts[1];
            $value['type'] = $REGISTER ;
         }
         else
            print ("Invalid field: $line\n");
      }
      return $value['type'];
}

$fields_by_name = array();
$field_dups = 0 ;
$registers_by_address = array();
$register_dups = 0 ;
$fieldsets = array();
$fieldset_usage = array();
$fieldsets_by_hash = array();
$fieldset_dups = 0 ;

function dedupe_field($field) {
   global $fields_by_name ;
   global $field_dups ;
   $name = $field['name'];
   $start = $field['startbit'];
   $stop  = $field['stopbit'];
   $fields = isset($fields_by_name[$name]) ? $fields_by_name[$name] : null ;
   if (is_array($fields)) {
      foreach ($fields as $f) {
         if (($start == $f['start'])&&($stop==$f['stop'])) {
            // print('matched:'); print_r($f);
            $field_dups++ ;
            return $f ;
         }
      }
   }
   $rval = array('name'=>$name,'start'=>$start,'stop'=>$stop);
   $fields_by_name[$name][] = $rval ;
   return $rval ;
}

function dedupe_register($reg) {
   global $registers_by_address ;
   global $register_dups ;
   $name = $reg['name'];
   $addr = $reg['address'];
   $width = $reg['width'];
   $regs = isset($registers_by_address[$addr]) ? $registers_by_address[$addr] : null ;
   if (is_array($regs)) {
      foreach ($regs as &$r) {
         if ($r['name'] == $name) {
            printf( "matched 0x%x:$name\n", $addr);
            $register_dups++ ;
            return $r ;
         }
      }
   } else
      $registers_by_address[$addr] = array();
   $idx = count($registers_by_address[$addr]);
   $registers_by_address[$addr][] = array('name'=>$name,'address'=>$addr,'width'=>$width);
   return $registers_by_address[$addr][$idx];
}

function field_sum($field_sum,$field){
   foreach ($field as $value) {
      $field_sum += crc32($value);
   }
   return $field_sum ;
}

function dedupe_fieldset(&$fs,$regname) {
   global $fieldsets_by_hash ;
   global $fieldsets ;
   global $fieldset_usage ;
   global $fieldset_dups ;
   $hash = array_reduce($fs,'field_sum',0);
   $fs['hash'] = $hash ;
   if (isset($fieldsets_by_hash[$hash])) {
      $fsets =& $fieldsets_by_hash[$hash];
      foreach($fsets as $fsetidx) {
         $fset =& $fieldsets[$fsetidx];
         if ($fset == $fs) {
            // print( "matched fieldset $hash:".count($fsets).":"); print_r($fs);
            $fieldset_dups++ ;
            $fieldset_usage[$fsetidx]++ ;
            return $fsetidx ;
         }
      }
   }
   else {
      $fieldsets_by_hash[$hash] = array();
   }
   $idx = count($fieldsets);
   $fieldsets[] = $fs ;
   $fieldset_usage[] = 1 ;
   $fieldsets_by_hash[$hash][] = $idx ;
   return $idx ;
}

function add_fields($reg,$fs) {
   global $registers_by_address ;
   global $register_dups ;
   $name = $reg['name'];
   $addr = $reg['address'];
   if (isset($registers_by_address[$addr])) {
      $regs =& $registers_by_address[$addr]; 
      foreach ($regs as &$r) {
         if ($r['name'] == $name) {
            $r['fields'] = $fs ;
            return ;
         }
      }
   } else
      print ("Error finding reg $reg[name].$reg[address]\n" );
}

if (isset($argc) && ($argc > 1)) {
   $fIn = file($argv[1]);
   print("$argv[1]: ".count($fIn)." lines\n");
   $numComments = 0 ;
   $lineTypes = array($COMMENT=>0,$REGISTER=>0,$FIELD=>0,$PARSEERR=>0);
   $prevreg = null ;
   $fieldset = array();
   foreach($fIn as $line) {
      $value= array();
      $type = parseLine($line,$value);
      $lineTypes[$type]++ ;
      if ($REGISTER == $type) {
         if (0 < count($fieldset)) {
            if (null !== $prevreg) {
               $fieldset =& dedupe_fieldset($fieldset,$prevreg['name']);
               if (isset($prevreg['fields'])) {
                  print( "register $prevreg[name] has fields\n");
               }
               add_fields($prevreg,$fieldset);
               $prevreg['fields'] =& $fieldset ;
               // print('prevreg: '); print_r($prevreg);
            } else
               print ("missing register at end of fieldset\n");
            $fieldset = array();
         }
         $prevreg =& dedupe_register($value);
//         print_r($value);
      } else if ($FIELD == $type) {
         $field =& dedupe_field($value);
         $fieldset[] = $field ;
//         print( "line $line: "); print_r($field);
      } else {
      }
   }
   if ((0 < count($fieldset)) && isset($prevreg)) {
      $fieldset =& dedupe_fieldset($fieldset,$prevreg['name']);
      if (isset($prevreg['fields'])) {
         print( "register $prevreg[name] has fields\n");
      }
      add_fields($prevreg,$fieldset);
      $prevreg['fields'] =& $fieldset ;
   }
   print_r($lineTypes);
   print("" .($lineTypes[0]+$lineTypes[1]+$lineTypes[2]+$lineTypes[3])." lines\n");
   print( "matched ".count($fields_by_name)." fields, $field_dups duplicates\n");
   print( "matched ".count($registers_by_address)." registers, $register_dups duplicates\n");
   print( "matched ".count($fieldsets_by_hash). " fieldsets, $fieldset_dups duplicates\n");
   for ($i = 0 ; $i < count($fieldsets); $i++) {
      $fields = $fieldsets[$i];
      $usage = $fieldset_usage[$i];
      if (1 < $usage) {
         print("/fs$i\t\t#usage $fieldset_usage[$i]\n");
         foreach ($fields as $idx => $field) {
            if (is_int($idx)) {
               $start = $field['start'];
               $stop  = $field['stop'];
               print ("\t:$field[name]:");
               if ($start != $stop)
                  printf ("%u-%u\n", $start, $stop);
               else
                  printf ("%u\n", $start);
            }
         }
      }
   }
   foreach ($registers_by_address as $addr => $addrregs) {
      foreach( $addrregs as $reg) {
         printf( "%-64s%s%s\n"
                  , $reg['name']
                  , $addr
                  , ('L' != $reg['width']) ? ('.'.$reg['width']) : '' );
         if (isset($reg['fields'])) {
            $idx = $reg['fields'];
            $usage = $fieldset_usage[$idx];
            if (1 == $usage) {
               $fields = $fieldsets[$idx];
               foreach ($fields as $idx => $field) {
                  if (is_int($idx)) {
                     $start = $field['start'];
                     $stop  = $field['stop'];
                     print ("\t:$field[name]:");
                     if ($start != $stop)
                        printf ("%u-%u\n", $start, $stop);
                     else
                        printf ("%u\n", $start);
                  }
               }
            } else {
               printf("\t:fs$idx/\n");
            }
         }
      }
   }
} else
   print( "Usage: $argv[0] inFile\n");
?>
