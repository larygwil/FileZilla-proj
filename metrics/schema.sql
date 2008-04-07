-- phpMyAdmin SQL Dump

SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";

--
-- Database: `code_metrics`
--

-- --------------------------------------------------------

--
-- Table structure for table `metrics`
--

CREATE TABLE IF NOT EXISTS `metrics` (
  `repository` tinyint(3) unsigned NOT NULL,
  `revision` mediumint(8) unsigned NOT NULL,
  `loc_header` mediumint(8) unsigned NOT NULL,
  `loc_source` mediumint(8) unsigned NOT NULL,
  `loc_other` mediumint(8) unsigned NOT NULL,
  `size_header` int(10) unsigned NOT NULL,
  `size_source` int(10) unsigned NOT NULL,
  `size_other` int(10) unsigned NOT NULL,
  `size_binary` int(10) unsigned NOT NULL,
  `count_header` smallint(5) unsigned NOT NULL,
  `count_source` smallint(5) unsigned NOT NULL,
  `count_other` smallint(5) unsigned NOT NULL,
  `count_binary` smallint(5) unsigned NOT NULL,
  PRIMARY KEY  (`repository`,`revision`),
  KEY `repository` (`repository`),
  KEY `revision` (`revision`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `repository`
--

CREATE TABLE IF NOT EXISTS `repository` (
  `id` tinyint(3) unsigned NOT NULL auto_increment,
  `name` varchar(20) NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 AUTO_INCREMENT=2 ;

-- --------------------------------------------------------

--
-- Table structure for table `revisions`
--

CREATE TABLE IF NOT EXISTS `revisions` (
  `repository` tinyint(3) unsigned NOT NULL,
  `revision` mediumint(8) unsigned NOT NULL,
  `date` datetime NOT NULL,
  PRIMARY KEY  (`repository`,`revision`),
  KEY `date` (`date`),
  KEY `revision` (`revision`),
  KEY `repository` (`repository`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Constraints for dumped tables
--

--
-- Constraints for table `metrics`
--
ALTER TABLE `metrics`
  ADD CONSTRAINT `metrics_ibfk_1` FOREIGN KEY (`repository`, `revision`) REFERENCES `revisions` (`repository`, `revision`) ON DELETE CASCADE ON UPDATE CASCADE;

--
-- Constraints for table `revisions`
--
ALTER TABLE `revisions`
  ADD CONSTRAINT `revisions_ibfk_1` FOREIGN KEY (`repository`) REFERENCES `repository` (`id`) ON DELETE CASCADE ON UPDATE CASCADE;

