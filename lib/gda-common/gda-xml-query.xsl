<!--      xml-query.dtd
          Copyright (C) 2001 Gerhard Dieringer
 
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
 
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
 
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 
-->

<xsl:stylesheet version="1.0"
	      xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	      xmlns="http://www.w3.org/TR/xhtml1/strict">

<xsl:strip-space elements="query select insert update delete"/>
<xsl:strip-space elements="targetlist target joinlist join on valueref"/>
<xsl:strip-space elements="union unionall intersect minus"/>
<xsl:strip-space elements="dest setlist set sourcelist valuelist value"/>
<xsl:strip-space elements="where having group arglist and or column order"/>
<xsl:strip-space elements="not eq ne lt le gt ge null field const func"/>

<xsl:output method="text" 
            encoding="iso-8859-1" 
            omit-xml-declaration="yes"
            version="1.0"/>

<xsl:template match="query">
   <xsl:apply-templates/>
   <xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="select">
  <xsl:text>select </xsl:text>
  <xsl:apply-templates select="valuelist"/>
  <xsl:apply-templates select="targetlist"/>
  <xsl:apply-templates select="where"/>
  <xsl:apply-templates select="having"/>
  <xsl:apply-templates select="group"/>
  <xsl:apply-templates select="union"/>
  <xsl:apply-templates select="unionall"/>
  <xsl:apply-templates select="intersect"/>
  <xsl:apply-templates select="minus"/>
  <xsl:apply-templates select="order"/>
</xsl:template>

<xsl:template match="insert">
  <xsl:text>insert into </xsl:text>
  <xsl:apply-templates select="target"/>
  <xsl:apply-templates select="dest"/>
  <xsl:apply-templates select="sourcelist"/>
</xsl:template>

<xsl:template match="update">
  <xsl:text>update </xsl:text>
  <xsl:apply-templates select="target"/>
  <xsl:apply-templates select="setlist"/>
  <xsl:apply-templates select="where"/>
</xsl:template>

<xsl:template match="delete">
  <xsl:text>delete from </xsl:text>
  <xsl:apply-templates select="target"/>
  <xsl:apply-templates select="where"/>
</xsl:template>

<xsl:template match="targetlist">
  <xsl:text> from </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="target">
  <xsl:value-of select="@name"/>
  <xsl:text> </xsl:text>
</xsl:template>

<xsl:template match="joinlist">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="join">
  <xsl:value-of select="@type"/>
  <xsl:text> join </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="on">
  <xsl:text> on </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="union">
  <xsl:text>union </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="unionall">
  <xsl:text>union all </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="intersect">
  <xsl:text>intersect </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="minus">
  <xsl:text>minus </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="dest">
  <xsl:text> (</xsl:text>
    <xsl:apply-templates/>
  <xsl:text>) </xsl:text>
</xsl:template>

<xsl:template match="setlist">
  <xsl:text>set </xsl:text>
  <xsl:call-template name="open-list"/>
</xsl:template>

<xsl:template match="set">
  <xsl:apply-templates select="./*[position()=1]"/>
  <xsl:text> = </xsl:text>
  <xsl:apply-templates select="./*[position()=2]"/>
</xsl:template>

<xsl:template match="sourcelist">
  <xsl:text> values (</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>) </xsl:text>
</xsl:template>

<xsl:template match="valuelist">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="value">
  <xsl:apply-templates/>
  <xsl:text> as </xsl:text>
  <xsl:value-of select="@id"/>  
  <xsl:if test="not(last()=position())">
    <xsl:text>, </xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="where">
  <xsl:text> where </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="having">
  <xsl:text> having </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="group">
  <xsl:text> group by </xsl:text>
  <xsl:call-template name="open-list"/>
</xsl:template>

<xsl:template match="order">
  <xsl:text> order by </xsl:text>
  <xsl:call-template name="open-list"/>
</xsl:template>

<xsl:template match="valueref">
  <xsl:apply-templates select="id(@source)/*"/>
</xsl:template>

<xsl:template match="arglist">
  <xsl:text>(</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>)</xsl:text>
</xsl:template>

<xsl:template match="not">
  <xsl:text>not(</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>)</xsl:text>
</xsl:template>


<xsl:template match="null">
  <xsl:text>(</xsl:text>
  <xsl:apply-templates/>
  <xsl:text> is null)</xsl:text>
</xsl:template>


<xsl:template match="field">
  <xsl:if test="string-length(@source)>0">
    <xsl:value-of select="concat(@source,'.')"/>
  </xsl:if>
  <xsl:value-of select="@name"/>
</xsl:template>

<xsl:template match="const">
  <xsl:if test="@type='char'">
    <xsl:text>'</xsl:text>
  </xsl:if>
  <xsl:value-of select="@value"/>
  <xsl:if test="@type='char'">
    <xsl:text>'</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="func">
  <xsl:value-of select="@name"/>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="sourcelist">
  <xsl:text> values </xsl:text>
  <xsl:call-template name="closed-list"/>
</xsl:template>

<xsl:template match="column">
  <xsl:value-of select="@num"/>
</xsl:template>

<xsl:template match="arglist|dest">
  <xsl:call-template name="closed-list"/>
</xsl:template>

<xsl:template match="eq">
  <xsl:call-template name="closed-list">
    <xsl:with-param name="delim" select="' = '"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="ne">
  <xsl:call-template name="closed-list">
    <xsl:with-param name="delim" select="' &lt;&gt; '"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="gt">
  <xsl:call-template name="closed-list">
    <xsl:with-param name="delim" select="' &gt; '"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="ge">
  <xsl:call-template name="closed-list">
    <xsl:with-param name="delim" select="' &gt;= '"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="lt">
  <xsl:call-template name="closed-list">
    <xsl:with-param name="delim" select="' &lt; '"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="le">
  <xsl:call-template name="closed-list">
    <xsl:with-param name="delim" select="' &lt;= '"/>
  </xsl:call-template>
</xsl:template>

<xsl:template match="and|or">
  <xsl:call-template name="closed-list">
    <xsl:with-param name="delim" select="concat(' ',name(.),' ')"/>
  </xsl:call-template>
</xsl:template>

<xsl:template name="closed-list">
  <xsl:param name="delim" select="', '"/>
  <xsl:text>(</xsl:text>
  <xsl:for-each select="./*">
    <xsl:apply-templates select="."/>
    <xsl:if test="not(position()=last())">
      <xsl:value-of select="$delim"/>
    </xsl:if>
  </xsl:for-each>
  <xsl:text>)</xsl:text>
</xsl:template>

<xsl:template name="open-list">
  <xsl:param name="delim" select="', '"/>
  <xsl:for-each select="./*">
    <xsl:apply-templates select="."/>
    <xsl:if test="not(position()=last())">
      <xsl:value-of select="$delim"/>
    </xsl:if>
  </xsl:for-each>
</xsl:template>


</xsl:stylesheet>

